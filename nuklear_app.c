#define WINDOW_WIDTH 510
#define WINDOW_HEIGHT 302

#define NK_ZERO_COMMAND_MEMORY
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
// #define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_QUICKDRAW_IMPLEMENTATION
// #define NK_BUTTON_TRIGGER_ON_RELEASE
#define NK_MEMSET memset
#define NK_MEMCPY memcpy

void aFailed(char *file, int line) {
    
    MoveTo(10, 10);
    char *textoutput;
    sprintf(textoutput, "%s:%d", file, line);
    writeSerialPortDebug(boutRefNum, "assertion failure");
    writeSerialPortDebug(boutRefNum, textoutput);
    // hold the program - we want to be able to read the text! assuming anything after the assert would be a crash
    while (true) {}
}

#define NK_ASSERT(e) \
    if (!(e)) \
        aFailed(__FILE__, __LINE__)

#include <Types.h>
#include "nuklear.h"
#include "nuklear_quickdraw.h"

struct nk_context *ctx;

void refreshNuklearApp(Boolean blankInput);

// UI setup and event handling goes here
static void nuklearApp(struct nk_context *ctx) {
    if (nk_begin(ctx, "Calculator", nk_rect(10, 10, 180, 240),
        NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE))
    {
        static int set = 0, prev = 0, op = 0;
        static const char numbers[] = "789456123";
        static const char ops[] = "+-*/";
        static double a = 0, b = 0;
        static double *current = &a;

        size_t i = 0;
        int solve = 0;
        {short len; char buffer[256];
        nk_layout_row_dynamic(ctx, 35, 1);
        len = sprintf(buffer, "%.2f", *current);
        // writeSerialPortDebug(boutRefNum, buffer);
        // char x[255];
        // sprintf(x, "%d", len);
        // writeSerialPortDebug(boutRefNum, x);
        // void nk_label(NkContext context, ByteBuffer text, int alignment)
        nk_edit_string(ctx, NK_EDIT_READ_ONLY, buffer, &len, 255, 0);
        buffer[len] = 0;
        *current = atof(buffer);}

        nk_layout_row_dynamic(ctx, 35, 4);
        for (i = 0; i < 16; ++i) {
            if (i >= 12 && i < 15) {
                if (i > 12) continue;
                if (nk_button_label(ctx, "C")) {
                    a = b = op = 0; current = &a; set = 0;
                } if (nk_button_label(ctx, "0")) {
                    *current = *current*10.0f; set = 0;
                } if (nk_button_label(ctx, "=")) {
                    solve = 1; prev = op; op = 0;
                }
            } else if (((i+1) % 4)) {
                if (nk_button_text(ctx, &numbers[(i/4)*3+i%4], 1)) {
                    *current = *current * 10.0f + numbers[(i/4)*3+i%4] - '0';
                    set = 0;
                }
            } else if (nk_button_text(ctx, &ops[i/4], 1)) {
                if (!set) {
                    if (current != &b) {
                        current = &b;
                    } else {
                        prev = op;
                        solve = 1;
                    }
                }
                op = ops[i/4];
                set = 1;
            }
        }
        if (solve) {
            if (prev == '+') a = a + b;
            if (prev == '-') a = a - b;
            if (prev == '*') a = a * b;
            if (prev == '/') a = a / b;
            current = &a;
            if (set) current = &b;
            b = 0; set = 0;
        }
    }
    nk_end(ctx);
}

void refreshNuklearApp(Boolean blankInput) {

    nk_input_begin(ctx);

    if (blankInput) {

        nk_input_key(ctx, NK_KEY_DEL, 1);
        nk_input_key(ctx, NK_KEY_DEL, 0);
    }

    nk_input_end(ctx);
    nuklearApp(ctx);
    nk_quickdraw_render(FrontWindow(), ctx);
    nk_clear(ctx);
}

struct nk_context* initializeNuklearApp() {

    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);
    refreshNuklearApp(false);

    return ctx;
}