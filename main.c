/*
* Author: Rustum Zia
* This program is created to generate fonts that can be easiliy
* rendered using Bubbl <https://github.com/ruuzia/bubbl> objects.
*
* The fractabubbler takes in a ttf font and a particular glyph and spits out
* an svg-conforming file containing only circles. This arrangement of
* circles of various sizes is designed to mimic the form of the glyph.
*
* How does it work?
* The mechanism and design of the fractabubbler is inspired by fractals such as
* the Apollonian Gasket <https://en.wikipedia.org/wiki/Apollonian_gasket>.
* It repeatedly finds the largest circle which can fit within the available space.
* Computing this position based on the joined Bezier curve segments which a font
* consists of appears mathematically terrifying. Instead I cheat by rasterizing
* the glyphs and performing a quadratic search through the bitmap repeatedly.
*/

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"


#define MAX_CIRCLE_RADIUS_PERCENT 0.2
#define DEFAULT_FINENESS 4
#define DEFAULT_HEIGHT 256

typedef struct {
    const char *font;
    int glyph;
    const char *output_file;

    // How small (in pixels) the circles can go to
    // A value of 1 would result in maximum coverage with pixel sized circles
    // A larger value would result in fewer circles with less detail
    int fineness;

    // Image height
    int height;

    bool debug_ascii_display;
} Program;

/* A greyscale bitmap */
typedef struct {
    uint8_t *data;
    int stride;
    int width;
    int height;
} Bitmap;

static inline int min(int a, int b) {
    return a < b ? a : b;
}

// Optimized midpoint circle algorithm
// https://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Jesko's_Method
static bool is_circle_in_image(Bitmap img, int cx, int cy, int r) {
    int x = r;  
    int y = 0;
    int t1 = r / 16;
    int t2 = 0;
    while (y <= x) {
        if (img.data[(cy + y) * img.stride + (cx + x)] == 0) return false;
        if (img.data[(cy - y) * img.stride + (cx + x)] == 0) return false;
        if (img.data[(cy + y) * img.stride + (cx - x)] == 0) return false;
        if (img.data[(cy - y) * img.stride + (cx - x)] == 0) return false;
        if (img.data[(cy + x) * img.stride + (cx + y)] == 0) return false;
        if (img.data[(cy + x) * img.stride + (cx - y)] == 0) return false;
        if (img.data[(cy - x) * img.stride + (cx + y)] == 0) return false;
        if (img.data[(cy - x) * img.stride + (cx - y)] == 0) return false;
        y++;
        t1 = t1 + y;
        t2 = t1 - x;
        if (t2 >= 0) {
            t1 = t2;
            x--;
        }
    }
    return true;
}

static double get_circle(const Bitmap img, int px, int py, int r) {
    if (px - r < 0 || px + r >= img.width
        || py - r < 0 || py + r >= img.height) return r-1;

    if (!is_circle_in_image(img, px, py, r)) return r-1;

    return get_circle(img, px, py, r+1);
}

static int find_biggest_circle(const Bitmap img, int *const out_x, int *const out_y) {
    double greatest_radius = 0;
    for (int x = 0; x < img.width; x++) {
        for (int y = 0; y < img.height; y++) {
            double r = get_circle(img, x, y, 0);
            if (r > greatest_radius) {
                greatest_radius = r;
                *out_x = x;
                *out_y = y;
            }
        }
    }
    return greatest_radius;
}


#define SIZE (1<<25)
stbtt_fontinfo font;
uint8_t ttf_buffer[SIZE];

Bitmap rasterize_glyph(const char *file_name, int c, int height) {
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

    uint8_t* bitmap = calloc(width*height, 1);

    int x0,y0,x1,y1;
    stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);
    /* printf("%d, %d, %d, %d\n", x0, y0, x1, y1); */

    int baseline = (int) (ascent*scale);
    stbtt_MakeCodepointBitmap(&font, &bitmap[(baseline + y0) * width + x0], x1-x0,y1-y0, width, scale,scale, c);

    return (Bitmap) {
        .data = bitmap,
        .stride = width,
        .width = width,
        .height = height,
    };
}

Bitmap make_bitmap(const Program program) {
    return rasterize_glyph(program.font, program.glyph, program.height);
}

void free_bitmap(Bitmap bitmap) {
    free(bitmap.data);
}

void display_ascii(Bitmap img) {
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            int c = " .:*|oO@"[img.data[y*img.stride + x]>>5];
            putchar(c);
            putchar(c);
        }
        putchar('\n');
    }
    fflush(stdout);
}

void fractabubble(const Program program, Bitmap img) {
    if (program.debug_ascii_display) {
        display_ascii(img);
    }

    FILE *svg = fopen(program.output_file, "w");
    fprintf(svg, "<?xml version=\"1.0\"?>\n");
    fprintf(svg, "<svg width=\"%d\" height=\"%d\">\n", img.width, img.height);

    int greatest_radius;
    int x_greatest, y_greatest;

    while ((greatest_radius = find_biggest_circle(img, &x_greatest, &y_greatest)) >= program.fineness) {
        const int p_x = x_greatest, p_y = y_greatest, r = greatest_radius;

        fprintf(svg, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#800080\" />\n", p_x, p_y, r);
        for (int j = -r; j < r; j++) {
            for (int i = -r; i < r; i++) {
                if (j*j + i*i < r*r) {
                    img.data[(p_y + j) * img.stride + (p_x + i)] = 0;
                }
            }
        }

        if (program.debug_ascii_display) {
            double seconds = 0.1;
            struct timespec req = { .tv_nsec = 1e9 * seconds };
            nanosleep(&req, NULL);
            display_ascii(img);
            fflush(stdout);
        }


    }

    fprintf(svg, "</svg>\n");
    fclose(svg);
}

static const char *arg0;
static void usage(int exitcode) {
    fprintf(stderr, "Usage:\n\t%s --font <file> --glyph <codepoint> --out <output> [--fineness <number>]\n", arg0);
    fprintf(stderr, "Example:\n\t%s --font fonts/LiberationSans-Regular.ttf --glyph 0x263a --out happy.svg --fineness 3\n", arg0);
    fprintf(stderr, "Specification:\n");
    fprintf(stderr, "\t--font <file>\n");
    fprintf(stderr, "\t\tLocal path to the ttf font file to use.\n");
    fprintf(stderr, "\t--glyph <codepoint>\n");
    fprintf(stderr, "\t\tUnicode codepoint to convert in hex (0x prefix), decimal, or octal (0 prefix) form.\n");
    fprintf(stderr, "\t--out <output>\n");
    fprintf(stderr, "\t\tOutput SVG file path.\n");
    fprintf(stderr, "\t[--fineness <number>]\n");
    fprintf(stderr, "\t\tDefault %d. How small the circles can get (1 = pixel fine).\n", DEFAULT_FINENESS);
    fprintf(stderr, "\t[--height <number>]\n");
    fprintf(stderr, "\t\tDefault %d. Height of the image.\n", DEFAULT_HEIGHT);
    exit(exitcode);
}

static const char *get_key(const char *item) {
    assert(item);
    if (item[0] != '-' || item[1] != '-') {
        fprintf(stderr, "Error: expected argument starting with dash(es), but got %s\n", item);
        usage(1);
    }
    return &item[2];
}

static const char *get_string(const char *item) {
    if (item == NULL) {
        fprintf(stderr, "Error: missing argument\n");
        usage(1);
    }
    return item;
}

static int get_number(const char *item) {
    item = get_string(item);
    int number = strtol(item, NULL, 0);
    if (number <= 0) {
        fprintf(stderr, "Error: expected positive decimal, octol, or hexadecimal literal (got %s)\n", item);
        usage(1);
    }
    return number;
}

static Program collect_args(char **argv) {
    arg0 = *argv++;
    char *item;
    Program args = {0};
    args.fineness = DEFAULT_FINENESS;
    args.height = DEFAULT_HEIGHT;
    while ((item = *argv++) != NULL) {
        const char *key = get_key(item);
        if (strcmp(key, "font") == 0) {
            args.font = get_string(*argv++);
        } else if (strcmp(key, "glyph") == 0) {
            args.glyph = get_number(*argv++);
        } else if (strcmp(key, "out") == 0) {
            args.output_file = get_string(*argv++);
        } else if (strcmp(key, "fineness") == 0) {
            args.fineness = get_number(*argv++);
        } else if (strcmp(key, "height") == 0) {
            args.height = get_number(*argv++);
        } else if (strcmp(key, "help") == 0) {
            usage(0);
        } else if (strcmp(key, "debug-ascii-display") == 0) {
            args.debug_ascii_display = true;
        } else {
            fprintf(stderr, "Error: unknown argument (%s)\n", key);
            usage(1);
        }
    }

    if (args.font == NULL) {
        fprintf(stderr, "Error: missing font\n");
        usage(1);
    }
    if (args.glyph == 0) {
        fprintf(stderr, "Error: missing glyph\n");
        usage(1);
    }
    if (args.output_file == NULL) {
        fprintf(stderr, "Error: missing output file\n");
        usage(1);
    }

    return args;
}

int main(int argc, char **argv) {
    (void)argc;
    const Program program = collect_args(argv);
    Bitmap bitmap = make_bitmap(program);
    fractabubble(program, bitmap);
    free_bitmap(bitmap);
}
