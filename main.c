#include <cairo/cairo-deprecated.h>
#include <math.h>
#include <stdio.h>
#include <cairo/cairo.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define FONT_WIDTH  0.6
#define FONT_HEIGHT 0.68

#define HEIGHT 256
#define WIDTH  ((int)(HEIGHT * FONT_WIDTH / FONT_HEIGHT))

#define MIN_RADIUS 5

typedef struct {
    uint8_t *data;
    int stride;
    int width;
    int height;
} Img;

static inline int is_within_bounds(const Img img, int x, int y) {
    return 0 <= x && x < img.width && 0 <= y && y < img.height;
}

static inline bool point_filled(const Img img, int x, int y) {
    return is_within_bounds(img, x, y) && img.data[x + y * img.stride];
}

static inline bool point_filled2(Img img, int x, int y) {
    bool result = is_within_bounds(img, x, y) && img.data[x + y * img.stride];
    if (result) img.data[x + y * img.stride] = 127;
    return result;
}
static int get_solid_radius2(const Img img, int x, int y) {
    int r;
    // For simplicity and performance, just check outward in
    // the four cardinal directions
    for (r = 0;
            point_filled2(img, x + r, y) &&
            point_filled2(img, x - r, y) &&
            point_filled2(img, x, y + r) &&
            point_filled2(img, x, y - r); r++) ;
    return r;
}

static int get_solid_radius(const Img img, int x, int y) {
    int r;
    // For simplicity and performance, just check outward in
    // the four cardinal directions
    for (r = 0;
            point_filled(img, x + r, y) &&
            point_filled(img, x - r, y) &&
            point_filled(img, x, y + r) &&
            point_filled(img, x, y - r); r++) ;
    return r;
}

static int find_greatest_radius(const Img img, int *const out_x, int *const out_y) {
    int greatest_radius = 0;
    for (int x = 0; x < img.width; x++) {
        for (int y = 0; y < img.height; y++) {
            int r = get_solid_radius(img, x, y);
            if (r > greatest_radius) {
                greatest_radius = r;
                *out_x = x;
                *out_y = y;
            }
        }
    }

    return greatest_radius;
}

void fractabubble(const char *glyph, const char *file_name) {
    // Create rasterized glyph
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_A8, WIDTH, HEIGHT);
    cairo_t *cr = cairo_create(surface);

    cairo_select_font_face(cr, "Liberation Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, HEIGHT / FONT_HEIGHT);
    cairo_move_to (cr, 0, HEIGHT);

    cairo_show_text(cr, glyph);
    cairo_surface_flush(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    Img img = {
        .data = cairo_image_surface_get_data(surface),
        .width = cairo_image_surface_get_width(surface),
        .height = cairo_image_surface_get_height(surface),
        .stride = cairo_image_surface_get_stride(surface),
    };

    FILE *svg = fopen(file_name, "w");
    fprintf(svg, "<?xml version=\"1.0\"?>\n");
    fprintf(svg, "<svg width=\"%d\" height=\"%d\">\n", img.width, img.height);

    int greatest_radius;
    while (true) {
        int x_greatest, y_greatest;
        greatest_radius = find_greatest_radius(img, &x_greatest, &y_greatest);
        if (greatest_radius < MIN_RADIUS) break;


        fprintf(svg, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#800080\" />\n", x_greatest, y_greatest, greatest_radius);

        cairo_move_to (cr, x_greatest, y_greatest);
        cairo_arc(cr, x_greatest, y_greatest, greatest_radius-1, 0, 2*M_PI);
        cairo_set_source_rgba (cr, 0, 0, 0, 0.0);
        cairo_fill(cr);
        cairo_surface_flush(surface);

    }

    fprintf(svg, "</svg>\n");
    fclose(svg);
    
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

int main(void) {
    for (char c = 'a'; c <= 'z'; c++) {
        char glyph[] = { c, '\0' };
        char file_name[256];
        sprintf(file_name, "glyphs/%c.svg", c);
        printf("%s :: %s\n", glyph, file_name);
        fractabubble(glyph, file_name);

    }

    return 0;
}
