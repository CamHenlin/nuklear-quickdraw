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
#include <Quickdraw.h>
#include <Scrap.h>
#include <Serial.h>
#include "SerialHelper.h"

typedef struct NkQuickDrawFont NkQuickDrawFont;
NK_API struct nk_context* nk_quickdraw_init(unsigned int width, unsigned int height);
NK_API int nk_quickdraw_handle_event(EventRecord *event, struct nk_context *nuklear_context);
NK_API void nk_quickdraw_shutdown(void);
NK_API void nk_quickdraw_render(WindowPtr window, struct nk_context *ctx);

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
#define NK_MEMSET memset
#define NK_MEMCPY memcpy
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

const Boolean NK_QUICKDRAW_GRAPHICS_DEBUGGING = false;
const Boolean NK_QUICKDRAW_EVENTS_DEBUGGING = false;

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
    
    // writeSerialPort(boutRefNum, "nk_quickdraw_font_get_text_width");

    if (!text) {

        return 0;
    }

    short output[len + 1];

    for (int i = 0; i <= len + 1; i++) {

        output[i] = 0;
    }

    float actualHeight = height - 2.0;

    TextSize(actualHeight);
    
    MeasureText(len, text, output); // TODO should be quickdraw PROCEDURE MeasureText (count: Integer; textAddr, charLocs: Ptr);
    // see inside macintosh: text pg 3-84
    
    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

        writeSerialPort(boutRefNum, "in nk_quickdraw- nk_quickdraw_font_get_text_width:");
        char log[1024];
        sprintf(log, "%s: %d -- %f", text, len, 1.0f * output[len]);
        writeSerialPort(boutRefNum, log);
    }

    return 1.0f * output[len] + 1; // adding the +4 for additional padding
}

/* Flags are identical to al_load_font() flags argument */
NK_API NkQuickDrawFont* nk_quickdraw_font_create_from_file() {

    NkQuickDrawFont *font = (NkQuickDrawFont*)calloc(1, sizeof(NkQuickDrawFont));

    font->font = calloc(1, 1024);
    if (font->font == NULL) {
        fprintf(stdout, "Unable to load font file\n");
        return NULL;
    }
    font->nk.userdata = nk_handle_ptr(font);
    font->nk.height = (int)14;
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
   
    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

       char stringMagicColorNumber[255];
       sprintf(stringMagicColorNumber, "stringMagicColorNumber: %f", magicColorNumber);
       writeSerialPort(boutRefNum, stringMagicColorNumber);
    }
   
   if (magicColorNumber > 37) {
       
       return blackColor;
   }
   
   return whiteColor;
}

// i split this in to a 2nd routine because we can use the various shades of gray when filling rectangles and whatnot
static Pattern nk_color_to_quickdraw_color(const struct nk_color *color) {

    // as a future upgrade, we could support color quickdraw
    // using an algorithm from https://stackoverflow.com/questions/3942878/how-to-decide-font-color-in-white-or-black-depending-on-background-color
    // if (red*0.299 + green*0.587 + blue*0.114) > 186 use #000000 else use #ffffff
    uint8_t red;
    uint8_t blue;
    uint8_t green;
    
    red = color->r;
    blue = color->b;
    green = color->g;
    
    float magicColorNumber = (uint8_t)red * 0.299 + (uint8_t)green * 0.587 + (uint8_t)blue * 0.114;

    if (magicColorNumber > 150) {

        return qd.black;
    } else if (magicColorNumber > 100) {

        return qd.dkGray;
    } else if (magicColorNumber > 75) {

        return qd.gray;
    } else if (magicColorNumber > 49) {

        return qd.ltGray;
    }

    return qd.white;
}

typedef struct {
    Ptr Address;
    long RowBytes;
    GrafPtr bits;
    Rect bounds;
    
    BitMap  BWBits;
    GrafPort BWPort;
    
    Handle  OrigBits;
    
} ShockBitmap;

void NewShockBitmap(ShockBitmap *theMap, short width, short height) {

    theMap->bits = 0L;
    SetRect(&theMap->bounds, 0, 0, width, height);
    
    theMap->BWBits.bounds = theMap->bounds;
    theMap->BWBits.rowBytes = ((width+15) >> 4)<<1;         // round to even
    theMap->BWBits.baseAddr = NewPtr(((long) height * (long) theMap->BWBits.rowBytes));

    theMap->BWBits.baseAddr = StripAddress(theMap->BWBits.baseAddr);
    
    OpenPort(&theMap->BWPort);
    SetPort(&theMap->BWPort);
    SetPortBits(&theMap->BWBits);

    SetRectRgn(theMap->BWPort.visRgn, theMap->bounds.left, theMap->bounds.top, theMap->bounds.right, theMap->bounds.bottom);
    SetRectRgn(theMap->BWPort.clipRgn, theMap->bounds.left, theMap->bounds.top, theMap->bounds.right, theMap->bounds.bottom);
    EraseRect(&theMap->bounds);
      
    theMap->Address = theMap->BWBits.baseAddr;
    theMap->RowBytes = (long) theMap->BWBits.rowBytes;
    theMap->bits = (GrafPtr) &theMap->BWPort;
}

ShockBitmap gMainOffScreen;

NK_API void nk_quickdraw_render(WindowPtr window, struct nk_context *ctx) {

    const struct nk_command *cmd = 0;

    OpenPort(&gMainOffScreen.BWPort);
    SetPort(&gMainOffScreen.BWPort);
    SetPortBits(&gMainOffScreen.BWBits);
    EraseRect(&gMainOffScreen.bounds);

    nk_foreach(cmd, ctx) {

        int color; // Color QuickDraw colors are integers - see Retro68/InterfacesAndLibraries/Interfaces&Libraries/Interfaces/CIncludes/Quickdraw.h:122 for more info

        switch (cmd->type) {

            case NK_COMMAND_NOP:
            
                
                if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                    writeSerialPort(boutRefNum, "NK_COMMAND_NOP");
                }
                

                break;
            case NK_COMMAND_SCISSOR: {
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_SCISSOR");
                    }

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
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_LINE");
                    }

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
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_RECT");
                    }

                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-102.html#MARKER-9-372
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-103.html#HEADING103-0
                    const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
                    color = nk_color_to_quickdraw_bw_color(r->color);
                    
                    ForeColor(color);
                    PenSize((float)r->line_thickness, (float)r->line_thickness);

                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)r->y;
                    quickDrawRectangle.left = (int)r->x;
                    quickDrawRectangle.bottom = (int)r->y + (int)r->h;
                    quickDrawRectangle.right = (int)r->x + (int)r->w;

                    FrameRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding);
                }

                break;
            case NK_COMMAND_RECT_FILLED: {
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_RECT_FILLED");
                    }

                    const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;

                    color = nk_color_to_quickdraw_bw_color(r->color);
                    
                    ForeColor(color);
                    Pattern colorPattern = nk_color_to_quickdraw_color(&r->color);

                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                    PenSize(1.0, 1.0); // no member line thickness on this struct so assume we want a thin line
                    // might actually need to build this with SetRect, search inside macintosh: imaging with quickdraw
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)r->y;
                    quickDrawRectangle.left = (int)r->x;
                    quickDrawRectangle.bottom = (int)r->y + (int)r->h;
                    quickDrawRectangle.right = (int)r->x + (int)r->w;

                    FillRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding, &colorPattern);
                    FrameRoundRect(&quickDrawRectangle, (float)r->rounding, (float)r->rounding); // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-105.html#HEADING105-0
                }

                break;
            case NK_COMMAND_CIRCLE: {
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_CIRCLE");
                    }

                    const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
                    color = nk_color_to_quickdraw_bw_color(c->color);
                    
                    ForeColor(color);  
                    
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)c->y;
                    quickDrawRectangle.left = (int)c->x;
                    quickDrawRectangle.bottom = (int)c->y + (int)c->h;
                    quickDrawRectangle.right = (int)c->x + (int)c->w;

                    FrameOval(&quickDrawRectangle); // An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                }

                break;
            case NK_COMMAND_CIRCLE_FILLED: {
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_CIRCLE_FILLED");
                    }

                    const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
                    
                    color = nk_color_to_quickdraw_bw_color(c->color);
                    
                    ForeColor(color);
                    Pattern colorPattern = nk_color_to_quickdraw_color(&c->color);
                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
                    PenSize(1.0, 1.0);
                    Rect quickDrawRectangle;
                    quickDrawRectangle.top = (int)c->y;
                    quickDrawRectangle.left = (int)c->x;
                    quickDrawRectangle.bottom = (int)c->y + (int)c->h;
                    quickDrawRectangle.right = (int)c->x + (int)c->w;

                    FillOval(&quickDrawRectangle, &colorPattern); 
                    FrameOval(&quickDrawRectangle);// An oval is a circular or elliptical shape defined by the bounding rectangle that encloses it. inside macintosh: imaging with quickdraw 3-25
                    // http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/QuickDraw/QuickDraw-111.html#HEADING111-0
                }

                break;
            case NK_COMMAND_TRIANGLE: {
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_TRIANGLE");
                    }

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
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_TRIANGLE_FILLED");
                    }

                    const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
                    Pattern colorPattern = nk_color_to_quickdraw_color(&t->color);
                    color = nk_color_to_quickdraw_bw_color(t->color);
                    PenSize(1.0, 1.0);
                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48
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
                
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_POLYGON");
                    }

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
                    
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_POLYGON_FILLED");
                    }

                    const struct nk_command_polygon *p = (const struct nk_command_polygon*)cmd;

                    Pattern colorPattern = nk_color_to_quickdraw_color(&p->color);
                    color = nk_color_to_quickdraw_bw_color(p->color);
                    // BackPat(&colorPattern); // inside macintosh: imaging with quickdraw 3-48 -- but might actually need PenPat -- look into this
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

                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_POLYLINE");
                    }

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
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_TEXT");
                        char log[255];
                        sprintf(log, "%f: %s, %d", (float)t->height, &t->string, (int)t->length);
                        writeSerialPort(boutRefNum, log);
                    }

                    // adjust font size down a bit from what nuklear specifies
                    // smaller fonts are pretty alright on mac given the low resolution,
                    // and nuklear wants everything in default 14pt which looks awful, whereas 12pt
                    // looks great
                    float fontHeight = (float)t->height - 2.0;

                    color = nk_color_to_quickdraw_bw_color(t->foreground);
                    ForeColor(color);
                    MoveTo((float)t->x, (float)t->y + fontHeight);
                    TextSize(fontHeight); 
                    DrawText((const char*)t->string, 0, (int)t->length);
                }

                break;
            case NK_COMMAND_CURVE: {
                    
                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_CURVE");
                    }

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

                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_ARC");
                    }

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

                    if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "NK_COMMAND_IMAGE");  
                    }

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
            
                if (NK_QUICKDRAW_GRAPHICS_DEBUGGING) {

                    writeSerialPort(boutRefNum, "NK_COMMAND_RECT_MULTI_COLOR/NK_COMMAND_ARC_FILLED/default");
                }
                break;
        }
    }

    SetPort(window);

    // our offscreen bitmap is the same size as our port rectangle, so we
    // get away with using the portRect sizing for source and destination
    CopyBits(&gMainOffScreen.bits->portBits, &window->portBits, &window->portRect, &window->portRect, srcCopy, 0L);
}

NK_API int nk_quickdraw_handle_event(EventRecord *event, struct nk_context *nuklear_context) { 
    // see: inside macintosh: toolbox essentials 2-4
    // and  inside macintosh toolbox essentials 2-79

    WindowPtr window;
    FindWindow(event->where, &window); 
    // char *logb;
    // sprintf(logb, "nk_quickdraw_handle_event event %d", event->what);
    // writeSerialPort(boutRefNum, logb);

    switch (event->what) {
        case updateEvt: {
                return 1;
            }
            break;
        case osEvt: { 
            // the quicktime osEvts are supposed to cover mouse movement events
            // notice that we are actually calling nk_input_motion in the EventLoop for the program
            // instead, as handling this event directly does not appear to work for whatever reason
            // TODO: research this
            writeSerialPort(boutRefNum, "osEvt");

                switch (event->message) {

                    case mouseMovedMessage: {

                        if (NK_QUICKDRAW_EVENTS_DEBUGGING) {
                            
                            writeSerialPort(boutRefNum, "mouseMovedMessage");
                        }


                        // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                        // TODO need to figure this out
                        nk_input_motion(nuklear_context, event->where.h, event->where.v); // TODO figure out mouse coordinates - not sure if this is right

                        break;
                    }

                    return 1;
                }
            }
            break;
        
        case mouseUp: 
            if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                writeSerialPort(boutRefNum, "mouseUp!!!");
            }
        case mouseDown: {
            if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                writeSerialPort(boutRefNum, "mouseUp/Down");
            }
            
            short part = FindWindow(event->where, &window);

			switch (part) {
                case inContent: {
                    // event->where should have coordinates??? or is it just a pointer to what the mouse is over?
                    // TODO need to figure this out
                    if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                        writeSerialPort(boutRefNum, "mouseUp/Down IN DEFAULT ZONE!!!!");
                    }

                    // this converts the offset of the window to the actual location of the mouse within the window
                    Point tempPoint;
                    SetPt(&tempPoint, event->where.h, event->where.v);
                    GlobalToLocal(&tempPoint);
                    
                    if (!event->where.h) {
                        
                        if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                            writeSerialPort(boutRefNum, "no event location for mouse!!!!");
                        }
                        return 1;
                    }
                    int x = tempPoint.h;
                    int y = tempPoint.v;

                    if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                        char *logx;
                        sprintf(logx, "xxxx h: %d,  v: %d", x, y);
                        writeSerialPort(boutRefNum, logx);
                    }

                    nk_input_motion(nuklear_context, x, y); // TODO we wouldnt need to do this if motion capturing was working right
                    nk_input_button(nuklear_context, NK_BUTTON_LEFT, x, y, event->what == mouseDown);
                }
                break;
                return 1;
            }
            
            break;
        case keyDown:
		case autoKey: {/* check for menukey equivalents */
                if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                    writeSerialPort(boutRefNum, "keyDown/autoKey");
                }

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
                } else {
                    
                    nk_input_unicode(nuklear_context, key);
                }

                return 1;
            }
			break;
        default: {
                if (NK_QUICKDRAW_EVENTS_DEBUGGING) {

                    writeSerialPort(boutRefNum, "default unhandled event");
                }
            
                return 1; 
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

    NewShockBitmap(&gMainOffScreen, width, height);
    NkQuickDrawFont *quickdrawfont = nk_quickdraw_font_create_from_file();
    struct nk_user_font *font = &quickdrawfont->nk;
    // nk_init_default(&quickdraw.nuklear_context, font);
    nk_init_default(&quickdraw.nuklear_context, font);
    nk_style_push_font(&quickdraw.nuklear_context, font);

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

    // fix styles to be more "mac-like"
    struct nk_style *style;
    struct nk_style_toggle *toggle;
    style = &quickdraw.nuklear_context.style;

    /* checkbox toggle */
    toggle = &style->checkbox;
    nk_zero_struct(*toggle);
    toggle->normal          = nk_style_item_color(nk_rgba(45, 45, 45, 255));
    toggle->hover           = nk_style_item_color(nk_rgba(80, 80, 80, 255)); // this is the "background" hover state regardless of checked status - we want light gray
    toggle->active          = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // i can't tell what this does yet
    toggle->cursor_normal   = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the "checked" box itself - we want "black"
    toggle->cursor_hover    = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the hover state of a "checked" box - anything lighter than black is ok
    toggle->userdata        = nk_handle_ptr(0);
    toggle->text_background = nk_rgba(255, 255, 255, 255);
    toggle->text_normal     = nk_rgba(70, 70, 70, 255);
    toggle->text_hover      = nk_rgba(70, 70, 70, 255);
    toggle->text_active     = nk_rgba(70, 70, 70, 255);
    toggle->padding         = nk_vec2(3.0f, 3.0f);
    toggle->touch_padding   = nk_vec2(0,0);
    toggle->border_color    = nk_rgba(0,0,0,0);
    toggle->border          = 0.0f;
    toggle->spacing         = 5;

    /* option toggle */
    toggle = &style->option;
    nk_zero_struct(*toggle);
    toggle->normal          = nk_style_item_color(nk_rgba(45, 45, 45, 255));
    toggle->hover           = nk_style_item_color(nk_rgba(80, 80, 80, 255)); // this is the "background" hover state regardless of checked status - we want light gray
    toggle->active          = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // i can't tell what this does yet
    toggle->cursor_normal   = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the "checked" box itself - we want "black"
    toggle->cursor_hover    = nk_style_item_color(nk_rgba(255, 255, 255, 255)); // this is the hover state of a "checked" box - anything lighter than black is ok
    toggle->userdata        = nk_handle_ptr(0);
    toggle->text_background = nk_rgba(255, 255, 255, 255);
    toggle->text_normal     = nk_rgba(70, 70, 70, 255);
    toggle->text_hover      = nk_rgba(70, 70, 70, 255);
    toggle->text_active     = nk_rgba(70, 70, 70, 255);
    toggle->padding         = nk_vec2(3.0f, 3.0f);
    toggle->touch_padding   = nk_vec2(0,0);
    toggle->border_color    = nk_rgba(0,0,0,0);
    toggle->border          = 0.0f;
    toggle->spacing         = 5;

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
    writeSerialPort(boutRefNum, "assertion failure");
    writeSerialPort(boutRefNum, textoutput);
    // hold the program - we want to be able to read the text! assuming anything after the assert would be a crash
    while (true) {}
}

#define NK_ASSERT(e) \
    if (!(e)) \
        aFailed(__FILE__, __LINE__)
        
