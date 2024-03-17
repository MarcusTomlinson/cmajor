#include "../../../include/cmajor/API/cmaj_Engine.h"
#include <fstream>
#include <iostream>

int main()
{
    auto engine = cmaj::Engine::create();

    cmaj::DiagnosticMessageList messages;

    cmaj::Program program;

    std::string code;

    int parallelBranches = 500;
    int branchNodes = 20;

    code += R"(
graph Test [[ main ]]
{
    input stream int in;
    output stream int out;
)";

    for ( int i = 0; i < parallelBranches; ++i )
    {
        for ( int j = 0; j < branchNodes; ++j )
        {
            code += R"(
    node passthrough)" +
                    std::to_string( i ) + "_" + std::to_string( j ) + " = Passthrough;";
        }
    }

    code += "\n";

    code += R"(
    connection
    {)";

    for ( int i = 0; i < parallelBranches; ++i )
    {
        code += R"(
        in -> passthrough)" +
                std::to_string( i ) + "_0;";
    }

    for ( int i = 0; i < parallelBranches; ++i )
    {
        for ( int j = 1; j < branchNodes; ++j )
        {
            code += R"(
        passthrough)" +
                    std::to_string( i ) + "_" + std::to_string( j - 1 ) + " -> passthrough" + std::to_string( i ) + "_" +
                    std::to_string( j ) + ";";
        }
    }

    for ( int i = 0; i < parallelBranches; ++i )
    {
        code += R"(
        passthrough)" +
                std::to_string( i ) + "_" + std::to_string( branchNodes - 1 ) + " -> out;";
    }

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
        std::cout << "Failed to parse!" << std::endl << messages.toString() << std::endl;
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
        if ( outputBlock.getSample( 0, 0 ) != inputData * parallelBranches )
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
