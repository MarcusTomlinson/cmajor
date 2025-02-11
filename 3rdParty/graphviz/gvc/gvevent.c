/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include "config.h"

#include "../gvc/gvplugin_layout.h"
#include "../gvc/gvcint.h"
#include "../gvc/gvcproc.h"
#include "../common/utils.h"

extern void emit_graph(GVJ_t * job, graph_t * g);
extern int gvLayout(GVC_t *gvc, graph_t *g, const char *engine);
extern int gvRenderFilename(GVC_t *gvc, graph_t *g, const char *format, const char *filename);
extern void graph_cleanup(graph_t *g);

#define PANFACTOR 10
#define ZOOMFACTOR 1.1
#define EPSILON .0001

static char *s_digraph = "digraph";
static char *s_graph = "graph";
static char *s_subgraph = "subgraph";
static char *s_node = "node";
static char *s_edge = "edge";
static char *s_tooltip = "tooltip";
static char *s_href = "href";
static char *s_URL = "URL";
static char *s_tailport = "tailport";
static char *s_headport = "headport";
static char *s_key = "key";

static void gv_graph_state(GVJ_t *job, graph_t *g)
{
    int j;
    Agsym_t *a;
    gv_argvlist_t *list;

    list = &(job->selected_obj_type_name);
    j = 0;
    if (g == agroot(g)) {
	if (agisdirected(g))
            gv_argvlist_set_item(list, j++, s_digraph);
	else
            gv_argvlist_set_item(list, j++, s_graph);
    }
    else {
        gv_argvlist_set_item(list, j++, s_subgraph);
    }
    gv_argvlist_set_item(list, j++, agnameof(g));
    list->argc = j;

    list = &(job->selected_obj_attributes);
    a = NULL;
    while ((a = agnxtattr(g, AGRAPH, a))) {
        gv_argvlist_set_item(list, j++, a->name);
        gv_argvlist_set_item(list, j++, agxget(g, a));
        gv_argvlist_set_item(list, j++, (char*)GVATTR_STRING);
    }
    list->argc = j;

    a = agfindgraphattr(g, s_href);
    if (!a)
	a = agfindgraphattr(g, s_URL);
    if (a)
	job->selected_href = strdup_and_subst_obj(agxget(g, a), g);
}

static void gv_node_state(GVJ_t *job, node_t *n)
{
    int j;
    Agsym_t *a;
    Agraph_t *g;
    gv_argvlist_t *list;

    list = &(job->selected_obj_type_name);
    j = 0;
    gv_argvlist_set_item(list, j++, s_node);
    gv_argvlist_set_item(list, j++, agnameof(n));
    list->argc = j;

    list = &(job->selected_obj_attributes);
    g = agroot(agraphof(n));
    a = NULL;
    while ((a = agnxtattr(g, AGNODE, a))) {
        gv_argvlist_set_item(list, j++, a->name);
        gv_argvlist_set_item(list, j++, agxget(n, a));
    }
    list->argc = j;

    a = agfindnodeattr(agraphof(n), s_href);
    if (!a)
        a = agfindnodeattr(agraphof(n), s_URL);
    if (a)
	job->selected_href = strdup_and_subst_obj(agxget(n, a), n);
}

static void gv_edge_state(GVJ_t *job, edge_t *e)
{
    int j;
    Agsym_t *a;
    Agraph_t *g;
    gv_argvlist_t *nlist, *alist;

    nlist = &(job->selected_obj_type_name);

    /* only tail, head, and key are strictly identifying properties,
     * but we commonly also use edge kind (e.g. "->") and tailport,headport
     * in edge names */
    j = 0;
    gv_argvlist_set_item(nlist, j++, s_edge);
    gv_argvlist_set_item(nlist, j++, agnameof(agtail(e)));
    j++; /* skip tailport slot for now */
    gv_argvlist_set_item(nlist, j++, (char*) (agisdirected(agraphof(agtail(e)))?"->":"--"));
    gv_argvlist_set_item(nlist, j++, agnameof(aghead(e)));
    j++; /* skip headport slot for now */
    j++; /* skip key slot for now */
    nlist->argc = j;

    alist = &(job->selected_obj_attributes);
    g = agroot(agraphof(aghead(e)));
    a = NULL;
    while ((a = agnxtattr(g, AGEDGE, a))) {

	/* tailport and headport can be shown as part of the name, but they
	 * are not identifying properties of the edge so we
	 * also list them as modifyable attributes. */
        if (strcmp(a->name,s_tailport) == 0)
	    gv_argvlist_set_item(nlist, 2, agxget(e, a));
	else if (strcmp(a->name,s_headport) == 0)
	    gv_argvlist_set_item(nlist, 5, agxget(e, a));

	/* key is strictly an identifying property to distinguish multiple
	 * edges between the same node pair.   Its non-writable, so
	 * no need to list it as an attribute as well. */
	else if (strcmp(a->name,s_key) == 0) {
	    gv_argvlist_set_item(nlist, 6, agxget(e, a));
	    continue;
	}

        gv_argvlist_set_item(alist, j++, a->name);
        gv_argvlist_set_item(alist, j++, agxget(e, a));
    }
    alist->argc = j;

    a = agfindedgeattr(agraphof(aghead(e)), s_href);
    if (!a)
	a = agfindedgeattr(agraphof(aghead(e)), s_URL);
    if (a)
	job->selected_href = strdup_and_subst_obj(agxget(e, a), e);
}

static void gvevent_refresh(GVJ_t * job)
{
    graph_t *g = job->gvc->g;

    if (!job->selected_obj) {
	job->selected_obj = g;
	GD_gui_state(g) |= GUI_STATE_SELECTED;
	gv_graph_state(job, g);
    }
    emit_graph(job, g);
    job->has_been_rendered = true;
}

/* recursively find innermost cluster containing the point */
static graph_t *gvevent_find_cluster(graph_t *g, boxf b)
{
    int i;
    graph_t *sg;
    boxf bb;

    for (i = 1; i <= GD_n_cluster(g); i++) {
	sg = gvevent_find_cluster(GD_clust(g)[i], b);
	if (sg)
	    return sg;
    }
    B2BF(GD_bb(g), bb);
    if (OVERLAP(b, bb))
	return g;
    return NULL;
}

static void * gvevent_find_obj(graph_t *g, boxf b)
{
    graph_t *sg;
    node_t *n;
    edge_t *e;

    /* edges might overlap nodes, so search them first */
    for (n = agfstnode(g); n; n = agnxtnode(g, n))
	for (e = agfstout(g, n); e; e = agnxtout(g, e))
	    if (overlap_edge(e, b))
	        return e;
    /* search graph backwards to get topmost node, in case of overlap */
    for (n = aglstnode(g); n; n = agprvnode(g, n))
	if (overlap_node(n, b))
	    return n;
    /* search for innermost cluster */
    sg = gvevent_find_cluster(g, b);
    if (sg)
	return sg;

    /* otherwise - we're always in the graph */
    return g;
}

static void gvevent_leave_obj(GVJ_t * job)
{
    void *obj = job->current_obj;

    if (obj) {
        switch (agobjkind(obj)) {
        case AGRAPH:
	    GD_gui_state((graph_t*)obj) &= (unsigned char)~GUI_STATE_ACTIVE;
	    break;
        case AGNODE:
	    ND_gui_state((node_t*)obj) &= (unsigned char)~GUI_STATE_ACTIVE;
	    break;
        case AGEDGE:
	    ED_gui_state((edge_t*)obj) &= (unsigned char)~GUI_STATE_ACTIVE;
	    break;
        }
    }
    job->active_tooltip = NULL;
}

static void gvevent_enter_obj(GVJ_t * job)
{
    void *obj;
    graph_t *g;
    edge_t *e;
    node_t *n;
    Agsym_t *a;

    free(job->active_tooltip);
    job->active_tooltip = NULL;
    obj = job->current_obj;
    if (obj) {
        switch (agobjkind(obj)) {
        case AGRAPH:
	    g = (graph_t*) obj;
	    GD_gui_state(g) |= GUI_STATE_ACTIVE;
	    a = agfindgraphattr(g, s_tooltip);
	    if (a)
		job->active_tooltip = strdup_and_subst_obj(agxget(g, a), obj);
	    break;
        case AGNODE:
	    n = (node_t*) obj;
	    ND_gui_state(n) |= GUI_STATE_ACTIVE;
	    a = agfindnodeattr(agraphof(n), s_tooltip);
	    if (a)
		job->active_tooltip = strdup_and_subst_obj(agxget(n, a), obj);
	    break;
        case AGEDGE:
	    e = (edge_t*) obj;
	    ED_gui_state(e) |= GUI_STATE_ACTIVE;
	    a = agfindedgeattr(agraphof(aghead(e)), s_tooltip);
	    if (a)
		job->active_tooltip = strdup_and_subst_obj(agxget(e, a), obj);
	    break;
        }
    }
}

static pointf pointer2graph (GVJ_t *job, pointf pointer)
{
    pointf p;

    /* transform position in device units to position in graph units */
    if (job->rotation) {
        p.x = pointer.y / (job->zoom * job->devscale.y) - job->translation.x;
        p.y = -pointer.x / (job->zoom * job->devscale.x) - job->translation.y;
    }
    else {
        p.x = pointer.x / (job->zoom * job->devscale.x) - job->translation.x;
        p.y = pointer.y / (job->zoom * job->devscale.y) - job->translation.y;
    }
    return p;
}

/* CLOSEENOUGH is in 1/72 - probably should be a feature... */
#define CLOSEENOUGH 1

static void gvevent_find_current_obj(GVJ_t * job, pointf pointer)
{
    void *obj;
    boxf b;
    double closeenough;
    pointf p;

    p =  pointer2graph (job, pointer);

    /* convert window point to graph coordinates */
    closeenough = CLOSEENOUGH / job->zoom;

    b.UR.x = p.x + closeenough;
    b.UR.y = p.y + closeenough;
    b.LL.x = p.x - closeenough;
    b.LL.y = p.y - closeenough;

    obj = gvevent_find_obj(job->gvc->g, b);
    if (obj != job->current_obj) {
	gvevent_leave_obj(job);
	job->current_obj = obj;
	gvevent_enter_obj(job);
	job->needs_refresh = true;
    }
}

static void gvevent_select_current_obj(GVJ_t * job)
{
    void *obj;

    obj = job->selected_obj;
    if (obj) {
        switch (agobjkind(obj)) {
        case AGRAPH:
	    GD_gui_state(obj) |= GUI_STATE_VISITED;
	    GD_gui_state(obj) &= (unsigned char)~GUI_STATE_SELECTED;
	    break;
        case AGNODE:
	    ND_gui_state(obj) |= GUI_STATE_VISITED;
	    ND_gui_state(obj) &= (unsigned char)~GUI_STATE_SELECTED;
	    break;
        case AGEDGE:
	    ED_gui_state(obj) |= GUI_STATE_VISITED;
	    ED_gui_state(obj) &= (unsigned char)~GUI_STATE_SELECTED;
	    break;
        }
    }

    free(job->selected_href);
    job->selected_href = NULL;

    obj = job->selected_obj = job->current_obj;
    if (obj) {
        switch (agobjkind(obj)) {
        case AGRAPH:
	    GD_gui_state(obj) |= GUI_STATE_SELECTED;
	    gv_graph_state(job, (graph_t*) obj);
	    break;
        case AGNODE:
	    ND_gui_state(obj) |= GUI_STATE_SELECTED;
	    gv_node_state(job, (node_t*) obj);
	    break;
        case AGEDGE:
	    ED_gui_state(obj) |= GUI_STATE_SELECTED;
	    gv_edge_state(job, (edge_t*) obj);
	    break;
        }
    }
}

static void gvevent_button_press(GVJ_t * job, int button, pointf pointer)
{
    switch (button) {
    case 1: /* select / create in edit mode */
	gvevent_find_current_obj(job, pointer);
	gvevent_select_current_obj(job);
        job->click = true;
	job->button = (unsigned char)button;
	job->needs_refresh = true;
	break;
    case 2: /* pan */
        job->click = true;
	job->button = (unsigned char)button;
	job->needs_refresh = true;
	break;
    case 3: /* insert node or edge */
	gvevent_find_current_obj(job, pointer);
        job->click = true;
	job->button = (unsigned char)button;
	job->needs_refresh = true;
	break;
    case 4:
        /* scrollwheel zoom in at current mouse x,y */
/* FIXME - should code window 0,0 point as feature with Y_GOES_DOWN */
        job->fit_mode = false;
        if (job->rotation) {
            job->focus.x -= (pointer.y - job->height / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.y);
            job->focus.y += (pointer.x - job->width / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.x);
        }
        else {
            job->focus.x += (pointer.x - job->width / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.x);
            job->focus.y += (pointer.y - job->height / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.y);
        }
        job->zoom *= ZOOMFACTOR;
        job->needs_refresh = true;
        break;
    case 5: /* scrollwheel zoom out at current mouse x,y */
        job->fit_mode = false;
        job->zoom /= ZOOMFACTOR;
        if (job->rotation) {
            job->focus.x += (pointer.y - job->height / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.y);
            job->focus.y -= (pointer.x - job->width / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.x);
        }
        else {
            job->focus.x -= (pointer.x - job->width / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.x);
            job->focus.y -= (pointer.y - job->height / 2.)
                    * (ZOOMFACTOR - 1.) / (job->zoom * job->devscale.y);
        }
        job->needs_refresh = true;
        break;
    }
    job->oldpointer = pointer;
}

static void gvevent_button_release(GVJ_t *job, int button, pointf pointer)
{
    (void)button;
    (void)pointer;

    job->click = false;
    job->button = false;
}

static void gvevent_motion(GVJ_t * job, pointf pointer)
{
    /* dx,dy change in position, in device independent points */
    double dx = (pointer.x - job->oldpointer.x) / job->devscale.x;
    double dy = (pointer.y - job->oldpointer.y) / job->devscale.y;

    if (fabs(dx) < EPSILON && fabs(dy) < EPSILON)  /* ignore motion events with no motion */
	return;

    switch (job->button) {
    case 0: /* drag with no button - */
	gvevent_find_current_obj(job, pointer);
	break;
    case 1: /* drag with button 1 - drag object */
	/* FIXME - to be implemented */
	break;
    case 2: /* drag with button 2 - pan graph */
	if (job->rotation) {
	    job->focus.x -= dy / job->zoom;
	    job->focus.y += dx / job->zoom;
	}
	else {
	    job->focus.x -= dx / job->zoom;
	    job->focus.y -= dy / job->zoom;
	}
	job->needs_refresh = true;
	break;
    case 3: /* drag with button 3 - drag inserted node or uncompleted edge */
	break;
    }
    job->oldpointer = pointer;
}

static int quit_cb(GVJ_t * job)
{
    (void)job;

    return 1;
}

static int left_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->focus.x += PANFACTOR / job->zoom;
    job->needs_refresh = true;
    return 0;
}

static int right_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->focus.x -= PANFACTOR / job->zoom;
    job->needs_refresh = true;
    return 0;
}

static int up_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->focus.y += -(PANFACTOR / job->zoom);
    job->needs_refresh = true;
    return 0;
}

static int down_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->focus.y -= -(PANFACTOR / job->zoom);
    job->needs_refresh = true;
    return 0;
}

static int zoom_in_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->zoom *= ZOOMFACTOR;
    job->needs_refresh = true;
    return 0;
}

static int zoom_out_cb(GVJ_t * job)
{
    job->fit_mode = false;
    job->zoom /= ZOOMFACTOR;
    job->needs_refresh = true;
    return 0;
}

static int toggle_fit_cb(GVJ_t * job)
{
/*FIXME - should allow for margins */
/*      - similar zoom_to_fit code exists in: */
/*      plugin/gtk/callbacks.c */
/*      plugin/xlib/gvdevice_xlib.c */
/*      lib/gvc/gvevent.c */

    job->fit_mode = !job->fit_mode;
    if (job->fit_mode) {
	/* FIXME - this code looks wrong */
	int dflt_width, dflt_height;
	dflt_width = job->width;
	dflt_height = job->height;
	job->zoom =
	    MIN((double) job->width / (double) dflt_width,
		(double) job->height / (double) dflt_height);
	job->focus.x = 0.0;
	job->focus.y = 0.0;
	job->needs_refresh = true;
    }
    return 0;
}

static void gvevent_read (GVJ_t * job, const char *filename, const char *layout)
{
    FILE *f;
    GVC_t *gvc;
    Agraph_t *g = NULL;
    gvlayout_engine_t *gvle;

    gvc = job->gvc;
    if (!filename) {
	g = agread(stdin,NULL);  // continue processing stdin
    }
    else {
	f = fopen(filename, "r");
	if (!f)
	   return;   /* FIXME - need some error handling */
	g = agread(f,NULL);
	fclose(f);
    }

    if (!g)
	return;   /* FIXME - need some error handling */

    if (gvc->g) {
	gvle = gvc->layout.engine;
	if (gvle && gvle->cleanup)
	    gvle->cleanup(gvc->g);
	graph_cleanup(gvc->g);
	agclose(gvc->g);
    }

    aginit (g, AGRAPH, "Agraphinfo_t", sizeof(Agraphinfo_t), TRUE);
    aginit (g, AGNODE, "Agnodeinfo_t", sizeof(Agnodeinfo_t), TRUE);
    aginit (g, AGEDGE, "Agedgeinfo_t", sizeof(Agedgeinfo_t), TRUE);
    gvc->g = g;
    GD_gvc(g) = gvc;
    if (gvLayout(gvc, g, layout) == -1)
	return;   /* FIXME - need some error handling */
    job->selected_obj = NULL;
    job->current_obj = NULL;
    job->needs_refresh = true;
}

static void gvevent_layout (GVJ_t * job, const char *layout)
{
    gvLayout(job->gvc, job->gvc->g, layout);
}

static void gvevent_render (GVJ_t * job, const char *format, const char *filename)
{
/* If gvc->jobs is set, a new job for doing the rendering won't be created.
 * If gvc->active_jobs is set, this will be used in a call to gv_end_job.
 * If we assume this function is called by an interactive front-end which
 * actually wants to write a file, the above possibilities can cause problems,
 * with either gvc->job being NULL or the creation of a new window. To avoid
 * this, we null out these values for rendering the file, and restore them
 * afterwards. John may have a better way around this.
 */
    GVJ_t* save_jobs;
    GVJ_t* save_active = NULL;
    if (job->gvc->jobs && (job->gvc->job == NULL)) {
	save_jobs = job->gvc->jobs;
	save_active = job->gvc->active_jobs;
	job->gvc->active_jobs = job->gvc->jobs = NULL;
    }
    else
	save_jobs = NULL;
    gvRenderFilename(job->gvc, job->gvc->g, format, filename);
    if (save_jobs) {
	job->gvc->jobs = save_jobs;
	job->gvc->active_jobs = save_active;
    }
}


gvevent_key_binding_t gvevent_key_binding[] = {
    {"Q", quit_cb},
    {"Left", left_cb},
    {"KP_Left", left_cb},
    {"Right", right_cb},
    {"KP_Right", right_cb},
    {"Up", up_cb},
    {"KP_Up", up_cb},
    {"Down", down_cb},
    {"KP_Down", down_cb},
    {"plus", zoom_in_cb},
    {"KP_Add", zoom_in_cb},
    {"minus", zoom_out_cb},
    {"KP_Subtract", zoom_out_cb},
    {"F", toggle_fit_cb},
};

int gvevent_key_binding_size = ARRAY_SIZE(gvevent_key_binding);

gvdevice_callbacks_t gvdevice_callbacks = {
    gvevent_refresh,
    gvevent_button_press,
    gvevent_button_release,
    gvevent_motion,
    NULL, // modify
    NULL, // del
    gvevent_read,
    gvevent_layout,
    gvevent_render,
};
