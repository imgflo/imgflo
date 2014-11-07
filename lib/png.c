//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <png.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *buffer;
  size_t size;
} PngEncoder;

void
write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    PngEncoder* p = (PngEncoder *)png_get_io_ptr(png_ptr);
    const size_t new_size = p->size + length;

    if(p->buffer) {
        p->buffer = g_realloc(p->buffer, new_size);
    } else {
        p->buffer = g_malloc(new_size);
    }
    g_assert(p->buffer);

    memcpy(p->buffer + p->size, data, length);
    p->size += length;
}

void
flush_data(png_structp png_ptr)
{
}

PngEncoder *
png_encoder_new(void) {
    PngEncoder *self = g_new(PngEncoder, 1);
    self->buffer = NULL;
    self->size = 0;
    return self;
}

void
png_encoder_free(PngEncoder *self) {
    if (self->buffer) {
        g_free(self->buffer);
    }
    g_free(self);
}

void
png_encoder_encode_rgba(PngEncoder *self, int width, int height, gchar *buffer) {
    g_return_if_fail(self);
    g_return_if_fail(width > 0);
    g_return_if_fail(height > 0);
    g_return_if_fail(buffer);

    png_byte color_type = PNG_COLOR_TYPE_RGBA;
    png_byte bit_depth = 8;

    png_bytep *row_pointers = (png_bytep *)g_malloc(sizeof(png_bytep) * height);
    const size_t bytes_per_row = 1*4*width;

    if (buffer) {
        for(int y = 0; y < height; y++) {
            const size_t offset = y*bytes_per_row;
            row_pointers[y] = (png_byte*)(buffer + offset);
        }
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        g_critical("[write_png_file] png_create_write_struct failed");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        g_critical("[write_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png_ptr)))
        g_critical("[write_png_file] Error during writing bytes");
    png_set_rows(png_ptr, info_ptr, row_pointers);

    //FILE *fp = fopen("pngtest2.png", "wb");
    FILE *fp = NULL;
    if (fp) {
        png_init_io(png_ptr, fp);
    } else {
        png_set_write_fn(png_ptr, self, write_data, flush_data);
    }
    if (setjmp(png_jmpbuf(png_ptr)))
        g_critical("[write_png_file] Error during init_io");

    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
        g_critical("[write_png_file] Error during writing header");
    png_set_IHDR(png_ptr, info_ptr, width, height,
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
        g_critical("[write_png_file] Error during writing bytes");
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
        g_critical("[write_png_file] Error during end of write");
    png_write_end(png_ptr, NULL);

    /*
    fp = fopen("pngtest2dir.png", "wb");
    size_t written = fwrite(self->buffer, 1, self->size, fp);
    if (written != self->size) {
        g_printerr("%s, written=%d, size=%d\n",
                   strerror(ferror(fp)), (int)written, (int)self->size);
    }
    if (fp) fclose(fp);
    */
}
