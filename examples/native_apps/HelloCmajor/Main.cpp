#include "../../../include/cmajor/API/cmaj_Engine.h"
#include <fstream>
#include <iostream>

int main()
{
    auto engine = cmaj::Engine::create();

    cmaj::DiagnosticMessageList messages;

    cmaj::Program program;

    std::string code;

    int componentCount = 10000;

    code += R"(
graph Test [[ main ]]
{
    input stream int in;
    output stream int out;
)";

    for ( int i = 0; i < componentCount; ++i )
    {
        code += R"(
    node passthrough)" +
                std::to_string( i ) + " = Passthrough;";
    }

    code += "\n";

    code += R"(
    connection
    {
        in -> passthrough0;)";

    for ( int i = 1; i < componentCount; ++i )
    {
        code += R"(
        passthrough)" +
                std::to_string( i - 1 ) + " -> passthrough" + std::to_string( i ) + ";";
    }

    code += R"(
        passthrough)" +
            std::to_string( componentCount - 1 ) + " -> out;";

    code += R"(
    }
}

processor Passthrough
    {
    input stream int in;
    output stream int out;

    void main()
    {
        loop
        {
            out <- in;
            advance();
        }
    }
}
)";

    std::ofstream out( "test.cmajor" );
    out << code;
    out.close();

    auto begin = std::chrono::high_resolution_clock::now();

    if ( !program.parse( messages, "internal", code ) )
    {
        return 1;
    }

    engine.setBuildSettings( cmaj::BuildSettings().setFrequency( 44100 ).setSessionID( 123456 ) );

    if ( !engine.load( messages, program, {}, {} ) )
    {
        return 1;
    }

    auto inputHandle = engine.getEndpointHandle( "in" );
    auto outputHandle = engine.getEndpointHandle( "out" );

    if ( !engine.link( messages ) )
    {
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto diff_ms = std::chrono::duration_cast<std::chrono::microseconds>( end - begin ).count() / 1000.0;
    std::cout << "Construction, 10000 Components: " << diff_ms << "ms\n";

    int inputData = 0;
    auto outputBlock = choc::buffer::InterleavedBuffer<int>( 1, 1 );

    auto performer = engine.createPerformer();
    performer.setBlockSize( 1 );

    const int iterationCount = 10000;

    begin = std::chrono::high_resolution_clock::now();

    for ( int i = 0; i < iterationCount; ++i )
    {
        ++inputData;
        performer.setInputFrames( inputHandle, &inputData, 1 );
        performer.advance();

        performer.copyOutputFrames( outputHandle, outputBlock );
        if ( outputBlock.getSample( 0, 0 ) != inputData )
        {
            std::cout << "Graph failed\n";
            return 1;
        }
    }

    end = std::chrono::high_resolution_clock::now();

    diff_ms = std::chrono::duration_cast<std::chrono::microseconds>( end - begin ).count() / 1000.0;
    std::cout << "10000 Components: " << diff_ms / iterationCount << "ms\n";

    return 0;
}
