/*
 * Nuklear - 1.32.0 - public domain
 * no warranty implied; use at your own risk.
 * based on allegro5 version authored from 2015-2016 by Micha Mettke
 * quickdraw version camhenlin 2021
 * 
 * v1 intent:
 * - only default system font support
 * - no graphics/images support - quickdraw has very limited support for this
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_QUICKDRAW_H_
#define NK_QUICKDRAW_H_
#include <MacTypes.h>
#include <Types.h>
#include <Resources.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <Scrap.h>
#include <SegLoad.h>
#include <Files.h>
#include <OSUtils.h>
#include <DiskInit.h>
#include <Packages.h>
#include <Traps.h>
#include <Devices.h>
#include <stdio.h>
#include <string.h>
//#include "nuklear.h"

typedef struct NkQuickDrawFont NkQuickDrawFont;
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height);
NK_API int nk_quickdraw_handle_event(EventRecord *event);
NK_API void nk_quickdraw_shutdown(void);
NK_API void nk_quickdraw_render(WindowPtr window);

NK_API struct nk_image* nk_quickdraw_create_image(const char* file_name);
NK_API void nk_quickdraw_del_image(struct nk_image* image);
NK_API NkQuickDrawFont* nk_quickdraw_font_create_from_file();

#endif

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */

#ifdef NK_QUICKDRAW_IMPLEMENTATION
#ifndef NK_QUICKDRAW_TEXT_MAX
#define NK_QUICKDRAW_TEXT_MAX 256
#endif

struct NkQuickDrawFont {
    struct nk_user_font nk;
    char *font;
};


// constant keyboard mappings for convenenience
// See Inside Macintosh: Text pg A-7, A-8
char homeKey = (char)0x01;
char enterKey = (char)0x03;
char endKey = (char)0x04;
char helpKey = (char)0x05;
char backspaceKey = (char)0x08;
char deleteKey = (char)0x7F;
char tabKey = (char)0x09;
char pageUpKey = (char)0x0B;
char pageDownKey = (char)0x0C;
char returnKey = (char)0x0D;
char rightArrowKey = (char)0x1D;
char leftArrowKey = (char)0x1C;
char downArrowKey = (char)0x1F;
char upArrowKey = (char)0x1E;
char eitherShiftKey = (char)0x0F;
char escapeKey = (char)0x1B;

// bezier code is from http://preserve.mactech.com/articles/mactech/Vol.05/05.01/BezierCurve/index.html
// as it is not built in to quickdraw like other "modern" graphics environments
/*
   The greater the number of curve segments, the smoother the curve, 
and the longer it takes to generate and draw.  The number below was pulled 
out of a hat, and seems to work o.k.
 */
#define SEGMENTS 16

static Fixed weight1[SEGMENTS + 1];
static Fixed weight2[SEGMENTS + 1];

#define w1(s) weight1[s]
#define w2(s) weight2[s]
#define w3(s) weight2[SEGMENTS - s]
#define w4(s) weight1[SEGMENTS - s]

/*
 *  SetupBezier  --  one-time setup code.
 * Compute the weights for the Bezier function.
 *  For the those concerned with space, the tables can be precomputed. 
Setup is done here for purposes of illustration.
 */
void setupBezier() {

    Fixed t, zero, one;
    int s;

    zero = FixRatio(0, 1);
    one = FixRatio(1, 1);
    weight1[0] = one;
    weight2[0] = zero;

    for (s = 1; s < SEGMENTS; ++s) {

        t = FixRatio(s, SEGMENTS);
        weight1[s] = FixMul(one - t, FixMul(one - t, one - t));
        weight2[s] = 3 * FixMul(t, FixMul(t - one, t - one));
    }

    weight1[SEGMENTS] = zero;
    weight2[SEGMENTS] = zero;
}

/*
 *  computeSegments  --  compute segments for the Bezier curve
 * Compute the segments along the curve.
 *  The curve touches the endpoints, so don’t bother to compute them.
 */
static void computeSegments(p1, p2, p3, p4, segment) Point  p1, p2, p3, p4; Point  segment[]; {

    int s;

    segment[0] = p1;

    for (s = 1; s < SEGMENTS; ++s) {

        segment[s].v = FixRound(w1(s) * p1.v + w2(s) * p2.v + w3(s) * p3.v + w4(s) * p4.v);
        segment[s].h = FixRound(w1(s) * p1.h + w2(s) * p2.h + w3(s) * p3.h + w4(s) * p4.h);
    }

    segment[SEGMENTS] = p4;
}

/*
 *  BezierCurve  --  Draw a Bezier Curve
 * Draw a curve with the given endpoints (p1, p4), and the given 
 * control points (p2, p3).
 *  Note that we make no assumptions about pen or pen mode.
 */
void BezierCurve(p1, p2, p3, p4) Point  p1, p2, p3, p4; {

    int s;
    Point segment[SEGMENTS + 1];

    computeSegments(p1, p2, p3, p4, segment);
    MoveTo(segment[0].h, segment[0].v);

    for (s = 1 ; s <= SEGMENTS ; ++s) {

        if (segment[s].h != segment[s - 1].h || segment[s].v != segment[s - 1].v) {

            LineTo(segment[s].h, segment[s].v);
        }
    }
}

// ex usage:
// Point   control[4] = {{144,72}, {72,144}, {216,144}, {144,216}};
// BezierCurve(c[0], c[1], c[2], c[3]);

static struct nk_quickdraw {
    unsigned int width;
    unsigned int height;
    struct nk_context nuklear_context;
    struct nk_buffer cmds;
} quickdraw;

// TODO: maybe V2 - skipping images for first pass
NK_API struct nk_image* nk_quickdraw_create_image(const char* file_name) {

    // TODO: just read the file to a char, we can draw it using quickdraw
    // b&w bitmaps are pretty easy to draw...
    // for us to do this, we need to figure out the format, then figure out if we can make it in to a quickdraw rect
    // and set the buffer to the image handle pointer in the image struct
    // i assume this gets consumed elsewhere in the code, thinking NK_COMMAND_IMAGE
    char* bitmap = ""; //al_load_bitmap(file_name); // TODO: https://www.allegro.cc/manual/5/al_load_bitmap, loads file to in-memory buffer understood by allegro

    if (bitmap == NULL) {

        fprintf(stdout, "Unable to load image file: %s\n", file_name);
        return NULL;
    }

    struct nk_image *image = (struct nk_image*)calloc(1, sizeof(struct nk_image));
    image->handle.ptr = bitmap;
    image->w = 0; // al_get_bitmap_width(bitmap); // TODO: this can be retrieved from a bmp file
    image->h = 0; // al_get_bitmap_height(bitmap); // TODO: this can be retrieved from a bmp file

    return image;
}

// TODO: maybe V2 - skipping images for first pass
NK_API void nk_quickdraw_del_image(struct nk_image* image) {

    if (!image) {
        
        return;
    }

    // al_destroy_bitmap(image->handle.ptr); // TODO: https://www.allegro.cc/manual/5/al_destroy_bitmap
    //this is just de-allocating the memory from a loaded bitmap

    free(image);
}

// TODO: fully convert
// TODO: assuming system font for v1, support other fonts in v2
static float nk_quickdraw_font_get_text_width(nk_handle handle, float height, const char *text, int len) {

    // NkAllegro5Font *font = (NkAllegro5Font*)handle.ptr;

    if (/*!font || */!text) {

        return 0;
    }

    /* We must copy into a new buffer with exact length null-terminated
       as nuklear uses variable size buffers and al_get_text_width doesn't
       accept a length, it infers length from null-termination
       (which is unsafe API design by allegro devs!) */
    char strcpy[len+1];
    strncpy((char*)&strcpy, text, len);
    strcpy[len] = '\0';
    int output[len + 1];
    
    MeasureText(len, text, output);// TODO not sure if this is right // al_get_text_width(font->font, strcpy); // TODO: look up what allegro was doing here to get the width of the text - could be easy if mac fonts are fixed length
    // TODO should be quickdraw PROCEDURE MeasureText (count: Integer; textAddr, charLocs: Ptr);
    // see inside macintosh: text pg 3-84
    
    return  output[len + 1];
}

/* Flags are identical to al_load_font() flags argument */
NK_API NkQuickDrawFont* nk_quickdraw_font_create_from_file() {

    NkQuickDrawFont *font = (NkQuickDrawFont*)calloc(1, sizeof(NkQuickDrawFont));

    font->font = 0x0;// = al_load_font(file_name, font_size, flags);
    if (font->font == NULL) {
        fprintf(stdout, "Unable to load font file\n");
        return NULL;
    }
    font->nk.userdata = nk_handle_ptr(font);
    font->nk.height = 12;
    font->nk.width = nk_quickdraw_font_get_text_width;
    return font;
}

static int nk_color_to_quickdraw_bw_color(struct nk_color color) {

    // TODO: since we are operating under a b&w display - we need to convert these colors to black and white
    // look up a simple algorithm for taking RGBA values and making the call on black or white and try it out here
    // as a future upgrade, we could support color quickdraw
    // using an algorithm from https://stackoverflow.com/questions/3942878/how-to-decide-font-color-in-white-or-black-depending-on-background-color
    // if (red*0.299 + green*0.587 + blue*0.114) > 186 use #000000 else use #ffffff
    // return al_map_rgba((unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b, (unsigned char)color.a);
    
    float magicColorNumber = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
    
    if (magicColorNumber > 186) {
        
        return blackColor;
    }
    
    return whiteColor;
}

// i split this in to a 2nd routine because we can use the various shades of gray when filling rectangles and whatnot
// i think these are technically patterns and not colors, see Quickdraw.h:2095
// not used yet but could be used on the pattern operations like boxes, etc
static Pattern nk_color_to_quickdraw_color(struct nk_color color) {

    // TODO: since we are operating under a b&w display - we need to convert these colors to black and white
    // look up a simple algorithm for taking RGBA values and making the call on black or white and try it out here
    // as a future upgrade, we could support color quickdraw
    // using an algorithm from https://stackoverflow.com/questions/3942878/how-to-decide-font-color-in-white-or-black-depending-on-background-color
    // if (red*0.299 + green*0.587 + blue*0.114) > 186 use #000000 else use #ffffff
    // return al_map_rgba((unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b, (unsigned char)color.a);
    
    float magicColorNumber = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
    short patternIdNumber;
    
    if (magicColorNumber > 200) {
        
        patternIdNumber = 1; //black; // TODO patterns are actually integer values - see Inside Macintosh: Imaging with QuickDraw: Figure 3-28
    } else if (magicColorNumber > 175) {
        
        patternIdNumber = 2; //dkGray;
    } else if (magicColorNumber > 150) {
        
        patternIdNumber = 3; //gray;
    } else if (magicColorNumber > 125) {
        
        patternIdNumber = 4; // ltGray;
    } else {
    
        patternIdNumber = 20; //white;
    }
    
    return (Pattern) **GetPattern(patternIdNumber);
}

NK_API void nk_quickdraw_render(WindowPtr window) {

    const struct nk_command *cmd;

    SetPort(window);

	EraseRect(&window->portRect);
    MoveTo(10, 10);
    ForeColor(blackColor);
    DrawText("hail satan", 0, 10);
    int i = 0;

    nk_foreach(cmd, &quickdraw.nuklear_context) {
        
        char *logMessage;
        sprintf(logMessage, "command: %d", i++);
        DrawText(logMessage, 0, strlen(logMessage));
        MoveTo(10, i * 10);

        int color; // Color QuickDraw colors are integers - see Retro68/InterfacesAndLibraries/Interfaces&Libraries/Interfaces/CIncludes/Quickdraw.h:122 for more info

        switch (cmd->type) {

            case NK_COMMAND_NOP:

                break;
            case NK_COMMAND_SCISSOR: {

                    const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
                    // al_set_clipping_rectangle((int)s->x, (int)s->y, (int)s->w, (int)s->h); // TODO: https://www.allegro.cc/manual/5/al_set_clipping_rectangle
                    // this essentially just sets the region of the screen that we are going to write to
                    // initially, i thought that this would be SetClip, but now believe this should be ClipRect, see:
                    // Inside Macintosh: Imaging with Quickdraw pages 2-48 and 2-49 for more info
                    // additionally, see page 2-61 for a data structure example for the rectangle OR 
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html
                    // for usage example
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)s->y;
                    quickDrawRectangle.left = (int)s->x;
                    quickDrawRectangle.bottom = (int)s->y + (int)s->h;
                    quickDrawRectangle.right = (int)s->x + (int)s->w;
                    
                    ClipRect(&quickDrawRectangle);
                }

                break;
            case NK_COMMAND_LINE: {

                    const struct nk_command_line *l = (const struct nk_command_line *)cmd;
                    color = nk_color_to_quickdraw_bw_color(l->color);
                    // great reference: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html
                    // al_draw_line((float)l->begin.x, (float)l->begin.y, (float)l->end.x, (float)l->end.y, color, (float)l->line_thickness); // TODO: look up and convert al_draw_line
                    ForeColor(color);
                    PenSize((float)l->line_thickness, (float)l->line_thickness);
                    MoveTo((float)l->begin.x, (float)l->begin.y);
                    LineTo((float)l->end.x, (float)l->end.y);
                }

                break;
            case NK_COMMAND_RECT: {

                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-102.html#MARKER-9-372
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-103.html#HEADING103-0
                    const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
                    color = nk_color_to_quickdraw_bw_color(r->color);
                    
                    ForeColor(color);
                    PenSize((float)r->line_thickness, (float)r->line_thickness);

                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)r->y;
                    quickDrawRectangle.left = (int)r->x;
                    quickDrawRectangle.bottom = (int)r->y - (int)r->h;
                    quickDrawRectangle.right = (int)r->x - (int)r->w;

                    FrameRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding);
                }

                break;
            case NK_COMMAND_RECT_FILLED: {

                    const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;

                    Pattern colorPattern = nk_color_to_quickdraw_color(r->color);

                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                    // PenSize((float)r->line_thickness, (float)r->line_thickness); no member line thickness on this struct

                    // might actually need to build this with SetRect, search inside macintosh: imaging with quickdraw
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)r->y;
                    quickDrawRectangle.left = (int)r->x;
                    quickDrawRectangle.bottom = (int)r->y - (int)r->h;
                    quickDrawRectangle.right = (int)r->x - (int)r->w;

                    FillRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding, &colorPattern); // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-105.html#HEADING105-0
                }

                break;
            case NK_COMMAND_CIRCLE: {

                    const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
                    color = nk_color_to_quickdraw_bw_color(c->color);
                    
                    ForeColor(color);  
                    
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)c->y;
                    quickDrawRectangle.left = (int)c->x;
                    quickDrawRectangle.bottom = (int)c->y - (int)c->h;
                    quickDrawRectangle.right = (int)c->x - (int)c->w;

                    FrameOval(&quickDrawRectangle); // An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                }

                break;
            case NK_COMMAND_CIRCLE_FILLED: {

                    const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
                    
                    Pattern colorPattern = nk_color_to_quickdraw_color(c->color);
                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48

                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)c->y;
                    quickDrawRectangle.left = (int)c->x;
                    quickDrawRectangle.bottom = (int)c->y - (int)c->h;
                    quickDrawRectangle.right = (int)c->x - (int)c->w;

                    FillOval(&quickDrawRectangle, &colorPattern); // An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-111.html#HEADING111-0
                }

                break;
            case NK_COMMAND_TRIANGLE: {

                    const struct nk_command_triangle *t = (const struct nk_command_triangle*)cmd;
                    color = nk_color_to_quickdraw_bw_color(t->color);
                    
                    ForeColor(color);
                    PenSize((float)t->line_thickness, (float)t->line_thickness);

                    MoveTo((float)t->a.x, (float)t->a.y);
                    LineTo((float)t->b.x, (float)t->b.y);
                    LineTo((float)t->c.x, (float)t->c.y);
                    LineTo((float)t->a.x, (float)t->a.y);
                }

                break;
            case NK_COMMAND_TRIANGLE_FILLED: {

                    const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
                    Pattern colorPattern = nk_color_to_quickdraw_color(t->color);
                    color = nk_color_to_quickdraw_bw_color(t->color);
                    BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                    ForeColor(color);

                    PolyHandle trianglePolygon = OpenPoly(); 
                    MoveTo((float)t->a.x, (float)t->a.y);
                    LineTo((float)t->b.x, (float)t->b.y);
                    LineTo((float)t->c.x, (float)t->c.y);
                    LineTo((float)t->a.x, (float)t->a.y);
                    ClosePoly();

                    FillPoly(trianglePolygon, &colorPattern);
                    KillPoly(trianglePolygon);
                }

                break;
            case NK_COMMAND_POLYGON: {

                    const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                    color = nk_color_to_quickdraw_bw_color(p->color);

                    int i;

                    for (i = 0; i < p->point_count; i++) {
                        
                        if (i == 0) {
                            
                            MoveTo(p->points[i].x, p->points[i].y);
                        }
                        
                        LineTo(p->points[i].x, p->points[i].y);
                        
                        if (i == p->point_count - 1) {
                            
                            
                            LineTo(p->points[0].x, p->points[0].y);
                        }
                    }
                }
                
                break;
            case NK_COMMAND_POLYGON_FILLED: {

                    const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                    Pattern colorPattern = nk_color_to_quickdraw_color(p->color);
                    color = nk_color_to_quickdraw_bw_color(p->color);
                    BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48 -- but might actually need PenPat -- look into this
                    ForeColor(color);

                    int i;

                    PolyHandle trianglePolygon = OpenPoly(); 
                    for (i = 0; i < p->point_count; i++) {
                        
                        if (i == 0) {
                            
                            MoveTo(p->points[i].x, p->points[i].y);
                        }
                        
                        LineTo(p->points[i].x, p->points[i].y);
                        
                        if (i == p->point_count - 1) {
                            
                            
                            LineTo(p->points[0].x, p->points[0].y);
                        }
                    }
                    
                    ClosePoly();

                    FillPoly(trianglePolygon, &colorPattern);
                    KillPoly(trianglePolygon);
                }
                
                break;
            case NK_COMMAND_POLYLINE: {

                    // this is similar to polygons except the polygon does not get closed to the 0th point
                    // check out the slight difference in the for loop
                    const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                    color = nk_color_to_quickdraw_bw_color(p->color);
                    ForeColor(color);

                    int i;

                    for (i = 0; i < p->point_count; i++) {
                        
                        if (i == 0) {
                            
                            MoveTo(p->points[i].x, p->points[i].y);
                        }
                        
                        LineTo(p->points[i].x, p->points[i].y);
                    }
                }

                break;
            case NK_COMMAND_TEXT: {

                    const struct nk_command_text *t = (const struct nk_command_text*)cmd;
                    color = nk_color_to_quickdraw_bw_color(t->foreground);
                    ForeColor(color);
                    MoveTo((float)t->x, (float)t->y);
                    DrawText((const char*)t->string, 0, strlen(t->string));
                }

                break;
            case NK_COMMAND_CURVE: {

                    const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
                    color = nk_color_to_quickdraw_bw_color(q->color);
                    ForeColor(color);
                    Point p1 = { (float)q->begin.x, (float)q->begin.y};
                    Point p2 = { (float)q->ctrl[0].x, (float)q->ctrl[0].y};
                    Point p3 = { (float)q->ctrl[1].x, (float)q->ctrl[1].y};
                    Point p4 = { (float)q->end.x, (float)q->end.y};

                    BezierCurve(p1, p2, p3, p4);
                }

                break;
            case NK_COMMAND_ARC: {

                    const struct nk_command_arc *a = (const struct nk_command_arc *)cmd;
                    color = nk_color_to_quickdraw_bw_color(a->color);
                    ForeColor(color);
                    
                    Rect arcBoundingBoxRectangle;
                    // this is kind of silly because the cx is at the center of the arc and we need to create a rectangle around it 
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-60.html#MARKER-2-116
                    float x1 = (float)a->cx - (float)a->r;
                    float y1 = (float)a->cy - (float)a->r;
                    float x2 = (float)a->cx + (float)a->r;
                    float y2 = (float)a->cy + (float)a->r;
                    SetRect(&arcBoundingBoxRectangle, x1, y1, x2, y2);
                    // SetRect(secondRect,90,20,140,70);

                    FrameArc(&arcBoundingBoxRectangle, a->a[0], a->a[1]);
                }

                break;
            case NK_COMMAND_IMAGE: {

                    const struct nk_command_image *i = (const struct nk_command_image *)cmd;
                    // al_draw_bitmap_region(i->img.handle.ptr, 0, 0, i->w, i->h, i->x, i->y, 0); // TODO: look up and convert al_draw_bitmap_region
                    // TODO: consider implementing a bitmap drawing routine. we could iterate pixel by pixel and draw
                    // here is some super naive code that could work, used for another project that i was working on with a custom format but would be
                    // easy to modify for standard bitmap files (just need to know how many bytes represent each pixel and iterate from there):
                    // 
                    // for (int i = 0; i < strlen(string); i++) {
                    //     printf("\nchar: %c", string[i]);
                    //     char pixel[1];
                    //     memcpy(pixel, &string[i], 1);
                    //     if (strcmp(pixel, "0") == 0) { // white pixel
                    //         MoveTo(++x, y);
                    //      } else if (strcmp(pixel, "1") == 0) { // black pixel
                    //          // advance the pen and draw a 1px x 1px "line"
                    //          MoveTo(++x, y);
                    //          LineTo(x, y);
                    //      } else if (strcmp(pixel, "|") == 0) { // next line
                    //          x = 1;
                    //          MoveTo(x, ++y);
                    //      } else if (strcmp(pixel, "/") == 0) { // end
                    //      }
                    //  }
                }
                
                break;
                
            // why are these cases not implemented?
            case NK_COMMAND_RECT_MULTI_COLOR:
            case NK_COMMAND_ARC_FILLED:
            default:

                break;
        }
    }

    nk_clear(&quickdraw.nuklear_context);
}

NK_API int nk_quickdraw_handle_event(EventRecord *event) { // see: inside macintosh: toolbox essentials 2-4

    struct nk_context *nuklear_context = &quickdraw.nuklear_context;
    WindowPtr	window;
    FindWindow(event->where, &window); 

    switch (event->what) {
        case osEvt: { // the quicktime osEvts cover mouse movement events

                switch (event->message) {

                    case mouseMovedMessage: {

                        // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                        // TODO need to figure this out
                        nk_input_motion(nuklear_context, event->where.h, event->where.v); // TODO figure out mouse coordinates - not sure if this is right

                        break;
                    }

                    return 1;
                }
            }
            break;
        case mouseDown: {
            
            short part = FindWindow(event->where, &window);

			switch (part) {
				case inMenuBar: {
                    
                    }
                    break;
				case inSysWindow: {			/* let the system handle the mouseDown */
                        SystemClick(event, window);
                    }
					break;
				case inDrag: {				/* pass screenBits.bounds to get all gDevices */
                        DragWindow(window, event->where, &qd.screenBits.bounds);
                    }
					break;
				case inZoomIn:
				case inZoomOut: {

                        Boolean hit = TrackBox(window, event->where, part);

                        if (hit) {

                            // this code mostly taken from example app in http://www.toughdev.com/content/2018/12/developing-68k-mac-apps-with-codelite-ide-retro68-and-pce-macplus-emulator/
                            SetPort(window);				/* the window must be the current port... */
                            EraseRect(&window->portRect);	/* because of a bug in ZoomWindow */
                            ZoomWindow(window, part, true);	/* note that we invalidate and erase... */
                            InvalRect(&window->portRect);	/* to make things look better on-screen */
                        }
                        
                    }
					break;
                default: {
                    // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                    // TODO need to figure this out
                    Boolean RELEASED = false; // todo i think we need to track thsi between events?
                    nk_input_button(nuklear_context, NK_BUTTON_LEFT, event->where.h, event->where.v, RELEASED /* TODO boolean */);
                }
                break;
                return 1;
            }
            
            break;
        case keyDown:
		case autoKey: {/* check for menukey equivalents */

                char key = event->message & charCodeMask;
                const Boolean keyDown = event->what == keyDown;

                if (event->modifiers & cmdKey) {/* Command key down */

                    if (keyDown) {

                        // AdjustMenus();						/* enable/disable/check menu items properly */
                        // DoMenuCommand(MenuKey(key));
                    }
                    
                    if (key == 'c') {
                        
                        nk_input_key(nuklear_context, NK_KEY_COPY, 1);
                    } else if (key == 'v') {
                        
                        nk_input_key(nuklear_context, NK_KEY_PASTE, 1);
                    } else if (key == 'x') {
                        
                        nk_input_key(nuklear_context, NK_KEY_CUT, 1);
                    } else if (key == 'z') {
                        
                        nk_input_key(nuklear_context, NK_KEY_TEXT_UNDO, 1);
                    } else if (key == 'r') {
                        
                        nk_input_key(nuklear_context, NK_KEY_TEXT_REDO, 1);
                    } else {
                        
                        nk_input_unicode(nuklear_context, key);
                    }
                } else if (key == eitherShiftKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_SHIFT, keyDown);
                } else if (key == deleteKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_DEL, keyDown);
                } else if (key == enterKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_ENTER, keyDown);
                } else if (key == tabKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_TAB, keyDown);
                } else if (key == leftArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_LEFT, keyDown);
                } else if (key == rightArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_RIGHT, keyDown);
                } else if (key == upArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_UP, keyDown);
                } else if (key == downArrowKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_DOWN, keyDown);
                } else if (key == backspaceKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_BACKSPACE, keyDown);
                } else if (key == escapeKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_TEXT_RESET_MODE, keyDown);
                } else if (key == pageUpKey) {
                 
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_UP, keyDown);
                } else if (key == pageDownKey) {
                    
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_DOWN, keyDown);
                } else if (key == homeKey) {

                    nk_input_key(nuklear_context, NK_KEY_TEXT_START, keyDown);
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_START, keyDown);
                } else if (key == endKey) {

                    nk_input_key(nuklear_context, NK_KEY_TEXT_END, keyDown);
                    nk_input_key(nuklear_context, NK_KEY_SCROLL_END, keyDown);
                }

                return 1;
            }
			break;
        default: {
            
                return 0; 
            }
            break;
        }
    }
}

// i think these functions are close to correct, but throw an error around invalid storage class
NK_INTERN void nk_quickdraw_clipboard_paste(nk_handle usr, struct nk_text_edit *edit) {    

    Handle hDest = NewHandle(0); //  { automatically resizes it}
    HLock(hDest);
    // {put data into memory referenced thru hDest handle}
    int sizeOfSurfData = GetScrap(hDest, 'TEXT', 0);
    HUnlock(hDest);
    nk_textedit_paste(edit, (char *)hDest, sizeOfSurfData);
    DisposeHandle(hDest);
}

NK_INTERN void nk_quickdraw_clipboard_copy(nk_handle usr, const char *text, int len) {

    // in Macintosh Toolbox the clipboard is referred to as "scrap manager"
    PutScrap(len, 'TEXT', text);
}

// it us up to our "main" function to call this code
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height) {

    // needed to calculate bezier info, see mactech article.
    setupBezier();

    NkQuickDrawFont *quickdrawfont = nk_quickdraw_font_create_from_file();
    struct nk_user_font *font = &quickdrawfont->nk;
    nk_init_default(&quickdraw.nuklear_context, font);

    // this is pascal code but i think we would need to do something like this if we want this function 
    // to be responsible for setting the window size
    //    Region locUpdateRgn = NewRgn();
    //    SetRect(limitRect, kMinDocSize, kMinDocSize, kMaxDocSize, kMaxDocSize);
    //    // {call Window Manager to let user drag size box}
    //    growSize = GrowWindow(thisWindow, event.where, limitRect);
    //    SizeWindow(thisWindow, LoWord(growSize), HiWord(growSize), TRUE);
    //    SectRect(oldViewRect, myData^^.editRec^^.viewRect, oldViewRect);
    //    // {validate the intersection (don't update)}
    //    ValidRect(oldViewRect);
    //    // {invalidate any prior update region}
    //    InvalRgn(locUpdateRgn);
    //    DisposeRgn(locUpdateRgn);

    quickdraw.nuklear_context.clip.copy = nk_quickdraw_clipboard_copy;
    quickdraw.nuklear_context.clip.paste = nk_quickdraw_clipboard_paste;
    quickdraw.nuklear_context.clip.userdata = nk_handle_ptr(0);

    return &quickdraw.nuklear_context;
}

NK_API void nk_quickdraw_shutdown(void) {

    nk_free(&quickdraw.nuklear_context);
    memset(&quickdraw, 0, sizeof(quickdraw));
}

#endif /* NK_QUICKDRAW_IMPLEMENTATION */
        
void aFailed(char *file, int line) {
    
    MoveTo(10, 10);
    char *textoutput;
    sprintf(textoutput, "%s:%d", file, line);
    DrawText(textoutput, 0, strlen(textoutput));
    // hold the program - we want to be able to read the text! assuming anything after the assert would be a crash
    while (true) {}
}

#define NK_ASSERT(e) \
    if (!(e)) \
        aFailed(__FILE__, __LINE__)