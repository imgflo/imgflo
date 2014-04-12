//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

typedef struct {
    gboolean running;
    GeglNode *node;
    guint monitor_id;
    GQueue *processing_queue; /* Queue of rectangles that needs to be processed */
    GeglRectangle *currently_processed_rect;
    GeglProcessor *processor;
} Processor;

static gboolean
task_monitor(Processor *self);

// TODO: also accept roi (x,y,w,h) and scale
Processor *
processor_new(void) {
    Processor *self = g_new(Processor, 1);
    self->running = FALSE;
    self->node = NULL;
    self->monitor_id  = 0;
    self->processor = NULL;
    self->processing_queue = g_queue_new();
    self->currently_processed_rect = NULL;
    return self;
}

void
processor_free(Processor *self) {
    g_free(self);
}

static void
trigger_processing(Processor *self, GeglRectangle roi)
{
    if (!self->node)
        return;

    if (self->monitor_id == 0) {
        self->monitor_id = g_idle_add_full(G_PRIORITY_LOW,
                           (GSourceFunc)task_monitor, self, NULL);
    }

    // Add the invalidated region to the dirty
    GeglRectangle *rect = g_new(GeglRectangle, 1);
    rect->x = roi.x;
    rect->y = roi.y;
    rect->width = roi.width;
    rect->height = roi.height;
    g_queue_push_head(self->processing_queue, rect);
}

static void
computed_event(GeglNode *node, GeglRectangle *rect, Processor *self)
{
    g_print("%s\n", __PRETTY_FUNCTION__);
}

static void
invalidated_event(GeglNode *node, GeglRectangle *rect, Processor *self)
{
    trigger_processing(self, *rect);
}

static gboolean
task_monitor(Processor *self)
{
    if (!self->processor || !self->node) {
        return FALSE;
    }

    // PERFORMANCE: combine all the rects added to the queue during a single
    // iteration of the main loop somehow

    if (!self->currently_processed_rect) {

        if (g_queue_is_empty(self->processing_queue)) {
            // Unregister worker
            self->monitor_id = 0;
            return FALSE;
        }
        else {
            // Fetch next rect to process
            self->currently_processed_rect = (GeglRectangle *)g_queue_pop_tail(self->processing_queue);
            g_assert(self->currently_processed_rect);
            gegl_processor_set_rectangle(self->processor, self->currently_processed_rect);
        }
    }

    gboolean processing_done = !gegl_processor_work(self->processor, NULL);

    if (processing_done) {
        // Go to next region
        if (self->currently_processed_rect) {
            g_free(self->currently_processed_rect);
        }
        self->currently_processed_rect = NULL;
    }

    return TRUE;
}

void
processor_set_running(Processor *self, gboolean running)
{
    if (self->running == running) {
        return;
    }
    self->running = running;

    if (self->running && self->node) {
        GeglRectangle bbox = gegl_node_get_bounding_box(self->node);
        self->processor = gegl_node_new_processor(self->node, &bbox);
        trigger_processing(self, bbox);
    }
}

void
processor_set_target(Processor *self, GeglNode *node)
{
    g_return_if_fail(self);

    if (TRUE) {
        self->node = node;
        return;
    }

    if (self->node == node) {
        return;
    }
    if (self->node) {
        g_object_unref(self->node);
    }
    if (node) {
        g_object_ref(node);
        self->node = node;

        g_signal_connect_object(self->node, "computed",
                                G_CALLBACK(computed_event),
                                self, 0);
        g_signal_connect_object(self->node, "invalidated",
                                G_CALLBACK(invalidated_event),
                                self, 0);

        if (self->processor) {
            g_object_unref(self->processor);
        }
        GeglRectangle bbox = gegl_node_get_bounding_box(self->node);
        self->processor = gegl_node_new_processor(self->node, &bbox);

        if (self->running) {
            trigger_processing(self, bbox);
        }

    } else {
        self->node = NULL;
    }
}
