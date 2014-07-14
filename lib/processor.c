//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

struct _Processor;

typedef void (* ProcessorInvalidatedCallback)
    (struct _Processor *processor, GeglRectangle rect, gpointer user_data);

typedef struct _Processor {
    gboolean running;
    GeglNode *node;
    guint monitor_id;
    GQueue *processing_queue; /* Queue of rectangles that needs to be processed */
    GeglRectangle *currently_processed_rect;
    GeglProcessor *processor;
    ProcessorInvalidatedCallback on_invalidated;
    gpointer on_invalidated_data;
    gint max_size;
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
    self->on_invalidated = NULL;
    self->on_invalidated_data = NULL;
    self->max_size = 1000; // Mainly to avoid DoS, or bugs causing out-of-memory
    return self;
}

void
processor_free(Processor *self) {
    g_free(self);
}

static GeglRectangle
sanitized_roi(Processor *self, GeglRectangle in) {
    GeglRectangle out = in;
    if (out.width > self->max_size) {
        g_warning("Processor: requested width exceeded max: %d", out.width);
        out.width = self->max_size;
    }
    if (out.height > self->max_size) {
        g_warning("Processor: requested height exceeded max: %d", out.height);
        out.height = self->max_size;
    }
    return out;
}

static void
trigger_processing(Processor *self, GeglRectangle roi)
{
    g_return_if_fail(self->node);

    if (self->on_invalidated) {
        self->on_invalidated(self, roi, self->on_invalidated_data);
    }

    if (!self->processor) {
        self->processor = gegl_node_new_processor(self->node, &roi);
        g_return_if_fail(self->processor);
    }

    if (self->monitor_id == 0) {
        self->monitor_id = g_idle_add_full(G_PRIORITY_LOW,
                           (GSourceFunc)task_monitor, self, NULL);
    }

    // Add the invalidated region to the dirty
    GeglRectangle *rect = g_new(GeglRectangle, 1);
    *rect = sanitized_roi(self, roi);
    if (rect->width >= 0 && rect->height >= 0) {
        g_queue_push_head(self->processing_queue, rect);
    }
}

static void
computed_event(GeglNode *node, GeglRectangle *rect, Processor *self)
{
    g_print("%s\n", __PRETTY_FUNCTION__);
}

static void
invalidated_event(GeglNode *node, GeglRectangle *rect, Processor *self)
{
    if (self->running) {
        trigger_processing(self, *rect);
    }
}

static gboolean
task_monitor(Processor *self)
{
    g_return_val_if_fail(self->processor, FALSE);
    g_return_val_if_fail(self->node, FALSE);

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
        trigger_processing(self, bbox);
    }
}

void
processor_set_target(Processor *self, GeglNode *node)
{
    g_return_if_fail(self);

    if (self->node == node) {
        return;
    }
    if (self->node) {
        g_object_unref(self->node);
    }
    if (node) {
        g_object_ref(node);
        self->node = node;

        g_signal_connect(self->node, "computed",
                        G_CALLBACK(computed_event), self);
        g_signal_connect(self->node, "invalidated",
                        G_CALLBACK(invalidated_event), self);

        if (self->processor) {
            g_object_unref(self->processor);
            self->processor = NULL;
        }

        if (self->running) {
            GeglRectangle bbox = gegl_node_get_bounding_box(self->node);
            trigger_processing(self, bbox);
        }

    } else {
        self->node = NULL;
    }
}

void
processor_blit(Processor *self, const Babl *format, GeglRectangle *roi_out, gchar **buffer_out) {
    g_return_if_fail(self);
    g_return_if_fail(buffer_out);
    g_return_if_fail(roi_out);

    GeglRectangle roi = gegl_node_get_bounding_box(self->node);
    roi = sanitized_roi(self, roi);
    // FIXME: take region-of-interest as parameter
    // FIXME: take size as parameter, set scale to give approx that
    const double scale = 1.0;
    gchar *buffer = g_malloc(roi.width*roi.height*babl_format_get_bytes_per_pixel(format));
    // XXX: maybe use GEGL_BLIT_DIRTY?
    gegl_node_blit(self->node, scale, &roi, format, buffer,
                   GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
    *buffer_out = buffer;
    *roi_out = roi;
}
