/*------------------------------------------------------------------------------##	Apple Macintosh Developer Technical Support##	MultiFinder-Aware Simple Sample Application##	Sample##	Sample.c	-	C Source##	Copyright © 1989 Apple Computer, Inc.#	All rights reserved.#*/#include <Types.h>#include <Resources.h>#include <Quickdraw.h>#include <Fonts.h>#include <Events.h>#include <Windows.h>#include <Menus.h>#include <TextEdit.h>#include <Dialogs.h>#include <ToolUtils.h>#include <Memory.h>#include <SegLoad.h>#include <Files.h>#include <OSUtils.h>#include <DiskInit.h>#include <Packages.h>#include <Traps.h>#include <Serial.h>#include <Devices.h>#include <stdio.h>#include <string.h>#include "Sample.h"#include "SerialHelper.h"#include "Quickdraw.h"#define WINDOW_WIDTH 300#define WINDOW_HEIGHT 200#define NK_INCLUDE_FIXED_TYPES#define NK_INCLUDE_STANDARD_IO#define NK_INCLUDE_STANDARD_VARARGS#define NK_INCLUDE_DEFAULT_ALLOCATOR#define NK_IMPLEMENTATION#define NK_QUICKDRAW_IMPLEMENTATION#include "nuklear.h"#include "nuklear_quickdraw.h"#define UNUSED(a) (void)a#define MIN(a,b) ((a) < (b) ? (a) : (b))#define MAX(a,b) ((a) < (b) ? (b) : (a))#define LEN(a) (sizeof(a)/sizeof(a)[0])//#include "../overview.c"/* The "g" prefix is used to emphasize that a variable is global. *//* GMac is used to hold the result of a SysEnvirons call. This makes   it convenient for any routine to check the environment. */SysEnvRec	gMac;				/* set up by Initialize *//* GHasWaitNextEvent is set at startup, and tells whether the WaitNextEvent   trap is available. If it is false, we know that we must call GetNextEvent. */Boolean		gHasWaitNextEvent;	/* set up by Initialize *//* GInBackground is maintained by our osEvent handling routines. Any part of   the program can check it to find out if it is currently in the background. */Boolean		gInBackground;		/* maintained by Initialize and DoEvent *//* The following globals are the state of the window. If we supported more than   one window, they would be attatched to each document, rather than globals. *//* GStopped tells whether the stop light is currently on stop or go. */Boolean		gStopped;			/* maintained by Initialize and SetLight *//* GStopRect and gGoRect are the rectangles of the two stop lights in the window. */Rect		gStopRect;			/* set up by Initialize */Rect		gGoRect;			/* set up by Initialize */Rect		gXRect;			/* set up by Initialize */struct nk_context *ctx;/* Here are declarations for all of the C routines. In MPW 3.0 we can use   actual prototypes for parameter type checking. */void EventLoop( struct nk_context ctx );void DoEvent( EventRecord *event );void AdjustCursor( Point mouse, RgnHandle region );void GetGlobalMouse( Point *mouse );void DoUpdate( WindowPtr window );void DoActivate( WindowPtr window, Boolean becomingActive );void DoContentClick( WindowPtr window );void DrawWindow( WindowPtr window );void AdjustMenus( void );void DoMenuCommand( long menuResult );void SetLight( WindowPtr window, Boolean newStopped );Boolean DoCloseWindow( WindowPtr window );void Terminate( void );void Initialize( void );Boolean GoGetRect( short rectID, Rect *theRect );void ForceEnvirons( void );Boolean IsAppWindow( WindowPtr window );Boolean IsDAWindow( WindowPtr window );Boolean TrapAvailable( short tNumber, TrapType tType );void AlertUser( void );/* Define HiWrd and LoWrd macros for efficiency. */#define HiWrd(aLong)	(((aLong) >> 16) & 0xFFFF)#define LoWrd(aLong)	((aLong) & 0xFFFF)/* Define TopLeft and BotRight macros for convenience. Notice the implicit   dependency on the ordering of fields within a Rect */#define TopLeft(aRect)	(* (Point *) &(aRect).top)#define BotRight(aRect)	(* (Point *) &(aRect).bottom)static void calculator(struct nk_context *ctx) {    if (nk_begin(ctx, "Calculator", nk_rect(10, 10, 180, 250),        NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE))    {        static int set = 0, prev = 0, op = 0;        static const char numbers[] = "789456123";        static const char ops[] = "+-*/";        static double a = 0, b = 0;        static double *current = &a;        size_t i = 0;        int solve = 0;        {int len; char buffer[256];        nk_layout_row_dynamic(ctx, 35, 1);        len = snprintf(buffer, 256, "%.2f", *current);        nk_edit_string(ctx, NK_EDIT_SIMPLE, buffer, &len, 255, nk_filter_float);        buffer[len] = 0;        *current = atof(buffer);}        nk_layout_row_dynamic(ctx, 35, 4);        for (i = 0; i < 16; ++i) {            if (i >= 12 && i < 15) {                if (i > 12) continue;                if (nk_button_label(ctx, "C")) {                    a = b = op = 0; current = &a; set = 0;                } if (nk_button_label(ctx, "0")) {                    *current = *current*10.0f; set = 0;                } if (nk_button_label(ctx, "=")) {                    solve = 1; prev = op; op = 0;                }            } else if (((i+1) % 4)) {                if (nk_button_text(ctx, &numbers[(i/4)*3+i%4], 1)) {                    *current = *current * 10.0f + numbers[(i/4)*3+i%4] - '0';                    set = 0;                }            } else if (nk_button_text(ctx, &ops[i/4], 1)) {                if (!set) {                    if (current != &b) {                        current = &b;                    } else {                        prev = op;                        solve = 1;                    }                }                op = ops[i/4];                set = 1;            }        }        if (solve) {            if (prev == '+') a = a + b;            if (prev == '-') a = a - b;            if (prev == '*') a = a * b;            if (prev == '/') a = a / b;            current = &a;            if (set) current = &b;            b = 0; set = 0;        }    }    nk_end(ctx);}#pragma segment Mainvoid main(){	    writeSerialPort(boutRefNum, "main screen turn on\n");	Initialize();					/* initialize the program */	UnloadSeg((Ptr) Initialize);	/* note that Initialize must not be in Main! */    struct nk_context *ctx;    writeSerialPort(boutRefNum, "call nk_init");    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);    writeSerialPort(boutRefNum, "call into event loop");	EventLoop(*ctx);					/* call the main event loop */}void nuklearEvents(EventRecord ev, struct nk_context ctx) {    writeSerialPort(boutRefNum, "nuklearEvents");    writeSerialPort(boutRefNum, "nk_input_begin");    nk_input_begin(&ctx);    writeSerialPort(boutRefNum, "nk_quickdraw_handle_event");    nk_quickdraw_handle_event(&ev);    writeSerialPort(boutRefNum, "nk_input_end");    nk_input_end(&ctx);    writeSerialPort(boutRefNum, "nk_begin");    /* GUI */    // calling nk_begin here will crash out unless we override NK_ASSERT and dont allow it to call through to assert.h//    if (nk_begin(&ctx, "Demo", nk_rect(0, 0, 200, 200), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {////////        enum {EASY, HARD};//        static int op = EASY;//        static int property = 20;////        nk_layout_row_static(&ctx, 30, 80, 1);////        if (nk_button_label(&ctx, "button")) {//            fprintf(stdout, "button pressed\n");//        }////        nk_layout_row_dynamic(&ctx, 30, 2);////        if (nk_option_label(&ctx, "easy", op == EASY)) {////            op = EASY;//        }////        if (nk_option_label(&ctx, "hard", op == HARD)) {////            op = HARD;//        }////        nk_layout_row_dynamic(&ctx, 22, 1);//        nk_property_int(&ctx, "Compression:", 0, &property, 100, 10, 1);//    }calculator(&ctx);        nk_quickdraw_render(FrontWindow(), &ctx);        writeSerialPort(boutRefNum, "nk_End");    //nk_end(&ctx);    writeSerialPort(boutRefNum, "back from nk_End");    /* -------------- EXAMPLES ---------------- */    //     // overview(&ctx);}/*	Get events forever, and handle them by calling DoEvent.	Get the events by calling WaitNextEvent, if it's available, otherwise	by calling GetNextEvent. Also call AdjustCursor each time through the loop. */#pragma segment Mainvoid EventLoop(struct nk_context ctx){	RgnHandle	cursorRgn;	Boolean		gotEvent;	EventRecord	event;	Point		mouse;	cursorRgn = NewRgn();			/* weÕll pass WNE an empty region the 1st time thru */	do {		/* use WNE if it is available */		if ( gHasWaitNextEvent ) {			GetGlobalMouse(&mouse);			AdjustCursor(mouse, cursorRgn);			gotEvent = WaitNextEvent(everyEvent, &event, MAXLONG, cursorRgn);		} else {			SystemTask();			gotEvent = GetNextEvent(everyEvent, &event);		}		if (gotEvent) {			/* make sure we have the right cursor before handling the event */			AdjustCursor(event.where, cursorRgn);			DoEvent(&event);		}        nuklearEvents(event, ctx);        		/*	If you are using modeless dialogs that have editText items,			you will want to call IsDialogEvent to give the caret a chance			to blink, even if WNE/GNE returned FALSE. However, check FrontWindow			for a non-NIL value before calling IsDialogEvent. */	} while ( true );	/* loop forever; we quit via ExitToShell */} /*EventLoop*//* Do the right thing for an event. Determine what kind of event it is, and call the appropriate routines. */#pragma segment Mainvoid DoEvent(event)	EventRecord	*event;{	short		part, err;	WindowPtr	window;	Boolean		hit;	char		key;	Point		aPoint;	switch ( event->what ) {		case mouseDown:			part = FindWindow(event->where, &window);			switch ( part ) {				case inMenuBar:				/* process a mouse menu command (if any) */					AdjustMenus();					DoMenuCommand(MenuSelect(event->where));					break;				case inSysWindow:			/* let the system handle the mouseDown */					SystemClick(event, window);					break;				case inContent:					if ( window != FrontWindow() ) {						SelectWindow(window);						/*DoEvent(event);*/	/* use this line for "do first click" */					} else						DoContentClick(window);					break;				case inDrag:				/* pass screenBits.bounds to get all gDevices */					DragWindow(window, event->where, &qd.screenBits.bounds);					break;				case inGrow:					break;				case inZoomIn:				case inZoomOut:					hit = TrackBox(window, event->where, part);					if ( hit ) {						SetPort(window);				/* the window must be the current port... */						EraseRect(&window->portRect);	/* because of a bug in ZoomWindow */						ZoomWindow(window, part, true);	/* note that we invalidate and erase... */						InvalRect(&window->portRect);	/* to make things look better on-screen */					}					break;			}			break;		case keyDown:		case autoKey:						/* check for menukey equivalents */			key = event->message & charCodeMask;			if ( event->modifiers & cmdKey )			/* Command key down */				if ( event->what == keyDown ) {					AdjustMenus();						/* enable/disable/check menu items properly */					DoMenuCommand(MenuKey(key));				}			break;		case activateEvt:			DoActivate((WindowPtr) event->message, (event->modifiers & activeFlag) != 0);			break;		case updateEvt:			DoUpdate((WindowPtr) event->message);			break;		/*	1.01 - It is not a bad idea to at least call DIBadMount in response			to a diskEvt, so that the user can format a floppy. */		case diskEvt:			if ( HiWord(event->message) != noErr ) {				SetPt(&aPoint, kDILeft, kDITop);				err = DIBadMount(aPoint, event->message);			}			break;		case kOSEvent:		/*	1.02 - must BitAND with 0x0FF to get only low byte */			switch ((event->message >> 24) & 0x0FF) {		/* high byte of message */				case kSuspendResumeMessage:		/* suspend/resume is also an activate/deactivate */					gInBackground = (event->message & kResumeMask) == 0;					DoActivate(FrontWindow(), !gInBackground);					break;			}			break;	}} /*DoEvent*//*	Change the cursor's shape, depending on its position. This also calculates the region	where the current cursor resides (for WaitNextEvent). If the mouse is ever outside of	that region, an event would be generated, causing this routine to be called,	allowing us to change the region to the region the mouse is currently in. If	there is more to the event than just Òthe mouse movedÓ, we get called before the	event is processed to make sure the cursor is the right one. In any (ahem) event,	this is called again before we 	fall back into WNE. */#pragma segment Mainvoid AdjustCursor(mouse,region)	Point		mouse;	RgnHandle	region;{	WindowPtr	window;	RgnHandle	arrowRgn;	RgnHandle	plusRgn;	Rect		globalPortRect;	window = FrontWindow();	/* we only adjust the cursor when we are in front */	if ( (! gInBackground) && (! IsDAWindow(window)) ) {		/* calculate regions for different cursor shapes */		arrowRgn = NewRgn();		plusRgn = NewRgn();		/* start with a big, big rectangular region */		SetRectRgn(arrowRgn, kExtremeNeg, kExtremeNeg, kExtremePos, kExtremePos);		/* calculate plusRgn */		if ( IsAppWindow(window) ) {			SetPort(window);	/* make a global version of the viewRect */			SetOrigin(-window->portBits.bounds.left, -window->portBits.bounds.top);			globalPortRect = window->portRect;			RectRgn(plusRgn, &globalPortRect);			SectRgn(plusRgn, window->visRgn, plusRgn);			SetOrigin(0, 0);		}		/* subtract other regions from arrowRgn */		DiffRgn(arrowRgn, plusRgn, arrowRgn);		/* change the cursor and the region parameter */		if ( PtInRgn(mouse, plusRgn) ) {			SetCursor(*GetCursor(plusCursor));			CopyRgn(plusRgn, region);		} else {			SetCursor(&qd.arrow);			CopyRgn(arrowRgn, region);		}		/* get rid of our local regions */		DisposeRgn(arrowRgn);		DisposeRgn(plusRgn);	}} /*AdjustCursor*//*	Get the global coordinates of the mouse. When you call OSEventAvail	it will return either a pending event or a null event. In either case,	the where field of the event record will contain the current position	of the mouse in global coordinates and the modifiers field will reflect	the current state of the modifiers. Another way to get the global	coordinates is to call GetMouse and LocalToGlobal, but that requires	being sure that thePort is set to a valid port. */#pragma segment Mainvoid GetGlobalMouse(mouse)	Point	*mouse;{	EventRecord	event;		OSEventAvail(kNoEvents, &event);	/* we aren't interested in any events */	*mouse = event.where;				/* just the mouse position */} /*GetGlobalMouse*//*	This is called when an update event is received for a window.	It calls DrawWindow to draw the contents of an application window.	As an effeciency measure that does not have to be followed, it	calls the drawing routine only if the visRgn is non-empty. This	will handle situations where calculations for drawing or drawing	itself is very time-consuming. */#pragma segment Mainvoid DoUpdate(window)	WindowPtr	window;{	if ( IsAppWindow(window) ) {		BeginUpdate(window);				/* this sets up the visRgn */		if ( ! EmptyRgn(window->visRgn) )	/* draw if updating needs to be done */			DrawWindow(window);		EndUpdate(window);	}} /*DoUpdate*//*	This is called when a window is activated or deactivated.	In Sample, the Window Manager's handling of activate and	deactivate events is sufficient. Other applications may have	TextEdit records, controls, lists, etc., to activate/deactivate. */#pragma segment Mainvoid DoActivate(window, becomingActive)	WindowPtr	window;	Boolean		becomingActive;{	if ( IsAppWindow(window) ) {		if ( becomingActive )			/* do whatever you need to at activation */ ;		else			/* do whatever you need to at deactivation */ ;	}} /*DoActivate*//*	This is called when a mouse-down event occurs in the content of a window.	Other applications might want to call FindControl, TEClick, etc., to	further process the click. */#pragma segment Mainvoid DoContentClick(window)	WindowPtr	window;{	SetLight(window, ! gStopped);} /*DoContentClick*//* Draw the contents of the application window. We do some drawing in color, using   Classic QuickDraw's color capabilities. This will be black and white on old   machines, but color on color machines. At this point, the windowÕs visRgn   is set to allow drawing only where it needs to be done. */static Rect				okayButtonBounds;void blitImage(window)	WindowPtr	window;{    char *string,*found;    int x = 1;    int y = 1;    SetPort(window);    ForeColor(blackColor);    PenSize(1,1);    MoveTo(1,1);    string = strdup("0101010101|1010101010|0101010101|1010101010|0101010101|1010101010|0101010101|1010101010|0101010101|1010101010/");    // printf("Original string: '%s'\n",string);    for (int i = 0; i < strlen(string); i++) {       // printf("\nchar: %c", string[i]);        char pixel[1];        memcpy(pixel, &string[i], 1);        if (strcmp(pixel, "0") == 0) { // white pixel            MoveTo(++x, y);        } else if (strcmp(pixel, "1") == 0) { // black pixel            // advance the pen and draw a 1px x 1px "line"            MoveTo(++x, y);            LineTo(x, y);        } else if (strcmp(pixel, "|") == 0) { // next line            x = 1;            MoveTo(x, ++y);        } else if (strcmp(pixel, "/") == 0) { // end        }    }}#pragma segment Mainvoid DrawWindow(window)	WindowPtr	window;{//	SetPort(window);//	EraseRect(&window->portRect);	/* clear out any garbage that may linger *///	if ( gStopped )					/* draw a red (or white) stop light *///		ForeColor(redColor);//	else//		ForeColor(whiteColor);////	PaintOval(&gStopRect);//	ForeColor(blackColor);//	FrameOval(&gStopRect);////	PaintOval(&gXRect);//	ForeColor(blackColor);//	FrameOval(&gXRect);////    PenSize(1,1);//    // PenPat(*blackColor);//    MoveTo(20,20);//    LineTo(70,20);//    LineTo(70,70);//    LineTo(20,70);//    LineTo(20,20);////    #define		kOkayButtPICTHiLit		153//    ClipRect(&window->portRect);//    PicHandle	thePict;//    thePict = GetPicture(kOkayButtPICTHiLit);//    MoveTo(0,0);//    DrawPicture(thePict, &window->portRect);//    ReleaseResource((Handle)thePict);// blitImage(window);////	if ( ! gStopped )				/* draw a green (or white) go light *///		ForeColor(greenColor);//	else//		ForeColor(whiteColor);//	PaintOval(&gGoRect);//	ForeColor(blackColor);//	FrameOval(&gGoRect);} /*DrawWindow*//*	Enable and disable menus based on the current state.	The user can only select enabled menu items. We set up all the menu items	before calling MenuSelect or MenuKey, since these are the only times that	a menu item can be selected. Note that MenuSelect is also the only time	the user will see menu items. This approach to deciding what enable/	disable state a menu item has the advantage of concentrating all	the decision-making in one routine, as opposed to being spread throughout	the application. Other application designs may take a different approach	that is just as valid. */#pragma segment Mainvoid AdjustMenus(){	WindowPtr	window;	MenuHandle	menu;	window = FrontWindow();	menu = GetMenuHandle(mFile);	if ( IsDAWindow(window) )		/* we can allow desk accessories to be closed from the menu */		EnableItem(menu, iClose);	else		DisableItem(menu, iClose);	/* but not our traffic light window */	menu = GetMenuHandle(mEdit);	if ( IsDAWindow(window) ) {		/* a desk accessory might need the edit menuÉ */		EnableItem(menu, iUndo);		EnableItem(menu, iCut);		EnableItem(menu, iCopy);		EnableItem(menu, iClear);		EnableItem(menu, iPaste);	} else {						/* Ébut we donÕt use it */		DisableItem(menu, iUndo);		DisableItem(menu, iCut);		DisableItem(menu, iCopy);		DisableItem(menu, iClear);		DisableItem(menu, iPaste);	}	menu = GetMenuHandle(mLight);	if ( IsAppWindow(window) ) {	/* we know that it must be the traffic light */		EnableItem(menu, iStop);		EnableItem(menu, iGo);	} else {		DisableItem(menu, iStop);		DisableItem(menu, iGo);	}	CheckItem(menu, iStop, gStopped); /* we can also determine check/uncheck state, too */	CheckItem(menu, iGo, ! gStopped);} /*AdjustMenus*//*	This is called when an item is chosen from the menu bar (after calling	MenuSelect or MenuKey). It performs the right operation for each command.	It is good to have both the result of MenuSelect and MenuKey go to	one routine like this to keep everything organized. */#pragma segment Mainvoid DoMenuCommand(menuResult)	long		menuResult;{	short		menuID;				/* the resource ID of the selected menu */	short		menuItem;			/* the item number of the selected menu */	short		itemHit;	Str255		daName;	short		daRefNum;	Boolean		handledByDA;	menuID = HiWord(menuResult);	/* use macros for efficiency to... */	menuItem = LoWord(menuResult);	/* get menu item number and menu number */	switch ( menuID ) {		case mApple:			switch ( menuItem ) {				// case iAbout:		/* bring up alert for About */                default:					itemHit = Alert(rAboutAlert, nil);					break;                /*				default:			// all non-About items in this menu are DAs					// type Str255 is an array in MPW 3					GetItem(GetMHandle(mApple), menuItem, daName);					daRefNum = OpenDeskAcc(daName);					break;                */			}			break;		case mFile:			switch ( menuItem ) {				case iClose:					DoCloseWindow(FrontWindow());					break;				case iQuit:					Terminate();					break;			}			break;		case mEdit:					/* call SystemEdit for DA editing & MultiFinder */			handledByDA = SystemEdit(menuItem-1);	/* since we donÕt do any Editing */			break;		case mLight:			switch ( menuItem ) {				case iStop:					SetLight(FrontWindow(), true);					break;				case iGo:					SetLight(FrontWindow(), false);					break;			}			break;        case mHelp:			switch ( menuItem ) {				case iQuickHelp:                    itemHit = Alert(rAboutAlert, nil);                        					break;				case iUserGuide:                {					// AlertUser();                    // write data to serial port                    // Configure PCE/macplus to map serial port to ser_b.out. This port can then be used for debug output                    // by using: tail -f ser_b.out                    // OSErr res = writeSerialPort(boutRefNum, "Hello World");                    // if (res < 0)                    //    AlertUser();                    // http://www.mac.linux-m68k.org/devel/macalmanac.php                    short* ROM85      = (short*) 0x028E;                     short* SysVersion = (short*) 0x015A;                     short* ScrVRes    = (short*) 0x0102;                     short* ScrHRes    = (short*) 0x0104;                     unsigned long*  Time       = (unsigned long*) 0x020C;                     char str2[255];                    sprintf(str2, "ROM85: %d - SysVersion: %d - VRes: %d - HRes: %d - Time: %lu", *ROM85, *SysVersion, *ScrVRes, *ScrHRes, *Time);                    // writeSerialPort(boutRefNum, str2);                                   Boolean is128KROM = ((*ROM85) > 0);                    Boolean hasSysEnvirons = false;                    Boolean hasStripAddr = false;                    Boolean hasSetDefaultStartup = false;                    if (is128KROM)                    {                        UniversalProcPtr trapSysEnv = GetOSTrapAddress(_SysEnvirons);                        UniversalProcPtr trapStripAddr = GetOSTrapAddress(_StripAddress);                        UniversalProcPtr trapSetDefaultStartup = GetOSTrapAddress(_SetDefaultStartup);                        UniversalProcPtr trapUnimpl = GetOSTrapAddress(_Unimplemented);                        hasSysEnvirons = (trapSysEnv != trapUnimpl);                        hasStripAddr = (trapStripAddr != trapUnimpl);                        hasSetDefaultStartup = (trapSetDefaultStartup != trapUnimpl);                    }                                        sprintf(str2, "is128KROM: %d - hasSysEnvirons: %d - hasStripAddr: %d - hasSetDefaultStartup - %d",                                     is128KROM, hasSysEnvirons, hasStripAddr, hasSetDefaultStartup);                    // writeSerialPort(boutRefNum, str2);               					break;                }			}			break;	}	HiliteMenu(0);					/* unhighlight what MenuSelect (or MenuKey) hilited */} /*DoMenuCommand*//* Change the setting of the light. */#pragma segment Mainvoid SetLight( window, newStopped )	WindowPtr	window;	Boolean		newStopped;{	if ( newStopped != gStopped ) {		gStopped = newStopped;		SetPort(window);		InvalRect(&window->portRect);	}} /*SetLight*//* Close a window. This handles desk accessory and application windows. *//*	1.01 - At this point, if there was a document associated with a	window, you could do any document saving processing if it is 'dirty'.	DoCloseWindow would return true if the window actually closed, i.e.,	the user didnÕt cancel from a save dialog. This result is handy when	the user quits an application, but then cancels the save of a document	associated with a window. */#pragma segment MainBoolean DoCloseWindow(window)	WindowPtr	window;{	/* if ( IsDAWindow(window) )		CloseDeskAcc(((WindowPeek) window)->windowKind);	else */ if ( IsAppWindow(window) )		CloseWindow(window);	return true;} /*DoCloseWindow*//***************************************************************************************** 1.01 DoCloseBehind(window) was removed ***	1.01 - DoCloseBehind was a good idea for closing windows when quitting	and not having to worry about updating the windows, but it suffered	from a fatal flaw. If a desk accessory owned two windows, it would	close both those windows when CloseDeskAcc was called. When DoCloseBehind	got around to calling DoCloseWindow for that other window that was already	closed, things would go very poorly. Another option would be to have a	procedure, GetRearWindow, that would go through the window list and return	the last window. Instead, we decided to present the standard approach	of getting and closing FrontWindow until FrontWindow returns NIL. This	has a potential benefit in that the window whose document needs to be saved	may be visible since it is the front window, therefore decreasing the	chance of user confusion. For aesthetic reasons, the windows in the	application should be checked for updates periodically and have the	updates serviced.**************************************************************************************//* Clean up the application and exit. We close all of the windows so that they can update their documents, if any. */ /*	1.01 - If we find out that a cancel has occurred, we won't exit to the	shell, but will return instead. */#pragma segment Mainvoid Terminate(){	WindowPtr	aWindow;	Boolean		closed;		closed = true;	do {		aWindow = FrontWindow();				/* get the current front window */		if (aWindow != nil)			closed = DoCloseWindow(aWindow);	/* close this window */		}	while (closed && (aWindow != nil));	if (closed)		ExitToShell();							/* exit if no cancellation */} /*Terminate*//*	Set up the whole world, including global variables, Toolbox managers,	and menus. We also create our one application window at this time.	Since window storage is non-relocateable, how and when to allocate space	for windows is very important so that heap fragmentation does not occur.	Because Sample has only one window and it is only disposed when the application	quits, we will allocate its space here, before anything that might be a locked	relocatable object gets into the heap. This way, we can force the storage to be	in the lowest memory available in the heap. Window storage can differ widely	amongst applications depending on how many windows are created and disposed. *//*	1.01 - The code that used to be part of ForceEnvirons has been moved into	this module. If an error is detected, instead of merely doing an ExitToShell,	which leaves the user without much to go on, we call AlertUser, which puts	up a simple alert that just says an error occurred and then calls ExitToShell.	Since there is no other cleanup needed at this point if an error is detected,	this form of error- handling is acceptable. If more sophisticated error recovery	is needed, an exception mechanism, such as is provided by Signals, can be used. */#pragma segment Initializevoid Initialize(){	Handle		menuBar;	WindowPtr	window;	long		total, contig;	EventRecord event;	short		count;	gInBackground = false;	InitGraf((Ptr) &qd.thePort);	InitFonts();	InitWindows();	InitMenus();	TEInit();	InitDialogs(nil);	InitCursor();	for (count = 1; count <= 3; count++)		EventAvail(everyEvent, &event);	window = (WindowPtr) NewPtr(sizeof(WindowRecord));	if ( window == nil ) AlertUser();	window = GetNewWindow(rWindow, (Ptr) window, (WindowPtr) -1);	menuBar = GetNewMBar(rMenuBar);			/* read menus into menu bar */	if ( menuBar == nil ) AlertUser();	SetMenuBar(menuBar);					/* install menus */	DisposeHandle(menuBar);	AppendResMenu(GetMenuHandle(mApple), 'DRVR');	/* add DA names to Apple menu */    	DrawMenuBar();		gStopped = true;					/* the go light rectangle */} /*Initialize*//*	Check to see if a window belongs to the application. If the window pointer	passed was NIL, then it could not be an application window. WindowKinds	that are negative belong to the system and windowKinds less than userKind	are reserved by Apple except for windowKinds equal to dialogKind, which	mean it is a dialog.	1.02 - In order to reduce the chance of accidentally treating some window	as an AppWindow that shouldn't be, we'll only return true if the windowkind	is userKind. If you add different kinds of windows to Sample you'll need	to change how this all works. */#pragma segment MainBoolean IsAppWindow(window)	WindowPtr	window;{	short		windowKind;	if ( window == nil )		return false;	else {	/* application windows have windowKinds = userKind (8) */		windowKind = ((WindowPeek) window)->windowKind;		return (windowKind = userKind);	}} /*IsAppWindow*//* Check to see if a window belongs to a desk accessory. */#pragma segment MainBoolean IsDAWindow(window)	WindowPtr	window;{	if ( window == nil )		return false;	else	/* DA windows have negative windowKinds */		return ((WindowPeek) window)->windowKind < 0;} /*IsDAWindow*//*	Check to see if a given trap is implemented. This is only used by the	Initialize routine in this program, so we put it in the Initialize segment.	The recommended approach to see if a trap is implemented is to see if	the address of the trap routine is the same as the address of the	Unimplemented trap. *//*	1.02 - Needs to be called after call to SysEnvirons so that it can check	if a ToolTrap is out of range of a pre-MacII ROM. */#pragma segment InitializeBoolean TrapAvailable(tNumber,tType)	short		tNumber;	TrapType	tType;{    	if ( ( tType == ToolTrap ) &&		( gMac.machineType > envMachUnknown ) &&		( gMac.machineType < envMacII ) ) {		/* it's a 512KE, Plus, or SE */		tNumber = tNumber & 0x03FF;		if ( tNumber > 0x01FF )					/* which means the tool traps */			tNumber = _Unimplemented;			/* only go to 0x01FF */	}	return NGetTrapAddress(tNumber, tType) != GetTrapAddress(_Unimplemented);} /*TrapAvailable*//*	Display an alert that tells the user an error occurred, then exit the program.	This routine is used as an ultimate bail-out for serious errors that prohibit	the continuation of the application. Errors that do not require the termination	of the application should be handled in a different manner. Error checking and	reporting has a place even in the simplest application. The error number is used	to index an 'STR#' resource so that a relevant message can be displayed. */#pragma segment Mainvoid AlertUser(){	short		itemHit;	SetCursor(&qd.arrow);	itemHit = Alert(rUserAlert, nil);	ExitToShell();} /* AlertUser */