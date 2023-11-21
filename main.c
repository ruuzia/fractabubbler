/*
* Author: Rustum Zia
* This script is created to generate fonts that can be easiliy
* rendered in Bubbl <https://github.com/ruuzia/bubbl>.
* What that is is a set of SVG files containing only circles.
*/

// TODO: use stb_truetype in place of cairo
// TODO: change script to be a program which only creates a single glyph
//       the font can then be managed and parallelized by another program

#include <math.h>
#include <stdio.h>
#include <cairo/cairo.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#define FONT_WIDTH  0.6
#define FONT_HEIGHT 0.68

#define WIDTH_HEIGHT_RATIO 0.6

#define HEIGHT 256
#define WIDTH  (HEIGHT * WIDTH_HEIGHT_RATIO)
#define BASE_HEIGHT 60

#define MIN_RADIUS 5
#define MAX_RADIUS 40

#define DBG_PNG

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

static double distsq(int x0, int y0, int x1, int y1) {
    int x = x1 - x0;
    int y = y1 - y0;
    return x*x + y*y;
}


static inline int min(int a, int b) {
    return a < b ? a : b;
}


// Optimization
//
static double cache_greatest_radius;

static double get_filled_radius2(const Img img, int px, int py) {
    // Shortcut
    if (!img.data[px + py*img.stride]) return 0;

    int room_left = px;
    int room_right = img.width - px - 1;
    int room_up = py;
    int room_down = img.height - py - 1;

    const double max_dist = min(min(min(min(cache_greatest_radius, room_left), room_right), room_down), room_up);

    double shortest_distsq = max_dist*max_dist;
    for (int x = px - max_dist; x <= px + max_dist; x++) {
        for (int y = py - max_dist; y <= py + max_dist; y++) {
            if (img.data[x + y*img.stride]) continue;
            double d = distsq(x, y, px, py);
            if (d < shortest_distsq) shortest_distsq = d;
        }
    }

    return sqrt(shortest_distsq);
}

static int find_greatest_radius(const Img img, int *const out_x, int *const out_y) {
    double greatest_radius = 0;
    for (int x = 0; x < img.width; x++) {
        for (int y = 0; y < img.height; y++) {
            double r = get_filled_radius2(img, x, y);
            if (r > greatest_radius) {
                greatest_radius = r;
                *out_x = x;
                *out_y = y;
            }
        }
    }
    cache_greatest_radius = greatest_radius;
    return greatest_radius;
}


#include <stdio.h>
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

#define SIZE (1<<25)
unsigned char ttf_buffer[SIZE];

Img rasterize(const char *file_name) {
    stbtt_fontinfo font;
    int c = 'a', height = 32;

    FILE* f = fopen(file_name, "rb");
    assert(f);

    fread(ttf_buffer, 1, SIZE, f);
    fclose(f);
    
    stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));
    float scale = stbtt_ScaleForPixelHeight(&font, height);

    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, NULL, NULL);

    int width;
    stbtt_GetCodepointHMetrics(&font, c, &width, NULL);
    width = (int)(width * scale);

    uint8_t* bitmap = malloc(width*height);

    int x0,y0,x1,y1;
    stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);
    /* printf("%d, %d, %d, %d", x0, y0, x1, y1); */

    int baseline = (int) (ascent*scale);
    stbtt_MakeCodepointBitmap(&font, &bitmap[(baseline + y0) * width + x0], x1-x0,y1-y0, width, scale,scale, c);

    return (Img) {
        .data = bitmap,
        .stride = width,
        .width = width,
        .height = height,
    };
}

int main(int argc, char **argv) {
    stbtt_fontinfo font;

    Img img = rasterize("/usr/share/fonts/liberation/Liberation-Regular.ttf");
    
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            putchar(" .:ioVM@"[img.data[y*img.stride + x]>>5]);
        }
        putchar('\n');
    }

    /* for (int y = 0; y < h; y++) { */
    /*     for (int x = 0; x < w; x++) { */
    /*         printf("%3d ", bitmap[y*w + x]); */
    /*     } */
    /*     putchar('\n'); */
    /* } */
    /* return 0; */
    free(img.data);
}

void fractabubble(const char *glyph, const char *file_name) {
    // Create rasterized glyph
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_A8, WIDTH, HEIGHT);
    cairo_t *cr = cairo_create(surface);

    cairo_select_font_face(cr, "Liberation Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, WIDTH / FONT_WIDTH);
    cairo_matrix_t matrix = {0};
    cairo_get_font_matrix(cr, &matrix);
    cairo_move_to (cr, 0, HEIGHT - BASE_HEIGHT);

    cairo_show_text(cr, glyph);
    cairo_surface_flush(surface);

#ifdef DBG_PNG
    cairo_surface_write_to_png(surface, "before.png");
#endif

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

    cache_greatest_radius = MAX_RADIUS;

    int greatest_radius;
    while (true) {
        int x_greatest, y_greatest;
        greatest_radius = find_greatest_radius(img, &x_greatest, &y_greatest);
        if (greatest_radius < MIN_RADIUS) break;


        fprintf(svg, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#800080\" />\n", x_greatest, y_greatest, greatest_radius-1);

        cairo_move_to (cr, x_greatest, y_greatest);
        cairo_arc(cr, x_greatest, y_greatest, greatest_radius-1, 0, 2*M_PI);
        cairo_set_source_rgba (cr, 0, 0, 0, 0.0);
        cairo_fill(cr);
        cairo_surface_flush(surface);

#ifdef DBG_PNG___
        cairo_surface_write_to_png(surface, "after.png");
        double seconds = 0.1;
        struct timespec req = { .tv_nsec = 1e9 * seconds };
        nanosleep(&req, NULL);
#endif
    }

    fprintf(svg, "</svg>\n");
    fclose(svg);

#ifdef DBG_PNG
    cairo_surface_write_to_png(surface, "after.png");
#endif
    
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

static FILE *atlas = NULL;

static void addToAtlas(const char *glyph, const char *file_name) {
    assert(atlas);
    printf("%s :: %s\n", glyph, file_name);
    fprintf(atlas, "\n%s\n%s\n", file_name, glyph);
}

static void named(const char *glyph, const char *name) {
    char file_name[256] = "glyphs/";
    strcat(file_name, name);
    strcat(file_name, ".svg");
    addToAtlas(glyph, file_name);
    fractabubble(glyph, file_name);
}

static void literal(char c) {
    char glyph[] = { c, '\0' };
    named(glyph, glyph);
}

int main1(void) {
    fractabubble("m", "a.svg");
    return 0;
}

int main0(void) {
    atlas = fopen("glyphs/atlas", "w");

    named(" ", "_space");
    named(".", "_period");
    named(":", "_colon");
    named(",", "_comma");
    named(";", "_semicolon");
    named("(", "_openparenthesis");
    named(")", "_closeparenthesis");
    named("[", "_opensquarebrackets");
    named("]", "_closesquarebrackets");
    named("*", "_star");
    named("!", "_exclamation");
    named("?", "_question");
    named("\'", "_singlequote");
    named("\"", "_doublequote");
    for (char c = '0'; c <= '9'; c++) literal(c);
    for (char c = 'a'; c <= 'z'; c++) literal(c);
    for (char c = 'A'; c <= 'Z'; c++) literal(c);

    fclose(atlas);
    return 0;
}
