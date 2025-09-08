/*
	GRAPHICS FUNCTIONS FOR GAME HEADER FILE
	---------------------------------------

	Creation Date:	24/01/93
	Author:		Stephen Eddy

	Revision History:


--------------------------------------------------------------------------
*/

// Legacy constants (DOS era) retained for reference. New 3DS code adds a modern
// interface further below guarded by PLATFORM_3DS.
const int SCRWIDTH=320;
const int SCRDEPTH=200;
const int SPRWIDTH=16;
const int SPRDEPTH=16;
const int ADDON=SCRWIDTH-SPRWIDTH;
const int UP=1;
const int DOWN=2;
const int LEFT=3;
const int RIGHT=4;
const int MORPH=5;
const int P1=0;
const int P2=1;
const int FALSE=0;
const int TRUE=1;

// Definition of palette structure to store a 256 colour palette
struct PalData {
	unsigned char Red[256];
	unsigned char Green[256];
	unsigned char Blue[256];
	};

typedef unsigned char uchar;
typedef unsigned int uint;


struct JoystickPosition {	// Structure for current Joystick settings
	uint    current_x;	// Current X
	uint	current_y;	// Current Y
	uchar	button_1;	// Status of Button 1
	uchar	button_2;	// Status of Button 2
	};

struct JoystickCalibrate {	// Structure for joystick maximums
	uint	min_x;		// Minimum X position
	uint	min_y;		// Minimum Y position
	uint	max_x;		// Maximum X position
	uint	max_y;		// Maximum Y position
	uint	x_centre;	// Centre X position
	uint	y_centre;	// Centre Y position
	};

struct KeyCodes {		// Structure for scancodes for movement keys
	uint	up_key;		// Scancode for UP key
	uint	down_key;	// Scancode for DOWN key
	uint	left_key;	// Scancode for LEFT key
	uint	right_key;	// Scancode for RIGHT key
	uint	morph_key;	// Scancode for MORPH key
	};

#ifndef PLATFORM_3DS
class PCX { // Legacy PCX loader (unused on 3DS build)
public:
	char *ImageData; // Pointer to image in memory
	PalData *Palette; // Palette for image
	PCX() { Palette = new PalData; }
	~PCX() { delete Palette; }
};
#endif

// Definition of second dimension of palette structure
//const unsigned char Red = 0;
//const unsigned char Green = 1;
//const unsigned char Blue = 2;
enum {
	PAGE0,
	PAGE1,
	PAGE2,
	PAGE3 };

#ifndef PLATFORM_3DS
// Hardware register and segment definitions (legacy DOS)
static unsigned char VideoSegment = 0xa000;
const int DacWrite = 0x03c8; // DacWrite register
const int DacRead  = 0x03c7; // DacRead register
const int DacData  = 0x03c9; // DacData register
const int InputStatus = 0x03da; // Input status register
const char VbiBit = 0x8; // Bit for vertical retrace interrupt
#endif

#ifndef PLATFORM_3DS
extern "C" {
void fillOffsets(void);
void blitSprite(int, int, int, int, char *);
void blitBrick(int, int, char *);
void blitBrickBack(int, int, char *, char *);
void eraseBrick(int, int, char *);
void eraseSprite(int, int, int, int, char *);
void blitBall(int, int, char *);
void eraseBall(int, int, char *);
void xblitSprite(int, int, int, int, int, int, int, int);
void xeraseSprite(int, int, int, int, int, char *);
void xcopyPage(int, int);
void blitMask(int, int, int, int, int, char *);
int getMaskPixel(int, int);
void xPutImage(char *, int);
void xSetPage(int);
int testMaskLine(int, int);
void setMaskPixel(int, int, int);
}
#endif

#ifndef PLATFORM_3DS
void SetModeX(void);
void xShowPage(void);
void xSwapPage(void);
void WritePlaneEnable(char);
void ReadPlaneEnable(char);
void WriteMode(char);
void LoadPalette(PalData *);
void ClearPalette(void);
void Palette2Grey(int, int);
void Fade2Palette(PalData *);
void Fade2Black(PalData *);
void Fade2Dark(PalData *, PalData *);
void FadeFromDark(PalData *, PalData *);
void DisplayImage(char *, char *);
void DisplayImageX(char *, int);
PCX *ReadImage(char *);
void GraphicsMode(void);
void TextMode(void);
void SetColour(char, char, char, char);
void WaitRetrace(void);
void DrawSprite(int, int, void *);
void SaveBack(int, int, void *);
void RestoreBack(int, int, void *);
void ReadJoystick(int);
void ROMFont(void);
void DrawText(char *, int, int, char);
void ClearBox(int, int, int, int, char);
void ClearScreen(void);
char ReadFire(int);
uint ReadPot(char);
void SetVMode(char mode);
void WaitRetrace(void);
int MouseReset(void);
int MouseMotionX(void);
int MouseMotionY(void);
int ReadMouse(int &, int &);
int MouseButton(void);
void DoBeep(int, int);
void NoBeep(void);
void SetupTimer(void);
void RestoreTimer(void);
void errhandler(char *, int);
void ProgramTimer0(int);
#endif

#ifdef PLATFORM_3DS
// ---------------------------------------------------------------------------
// Modern 3DS platform abstraction (citro2d / citro3d bottom-screen only)
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <citro2d.h>

struct InputState {
	int  stylusX = -1;
	int  stylusY = -1;
	bool touching = false;
	bool touchPressed = false; // edge: became touching this frame
	bool fireHeld = false; // D-Pad Up
	bool startPressed = false; // START key (edge)
	bool selectPressed = false; // SELECT key (edge)
	bool aPressed = false; // A key (edge)
	bool bPressed = false; // B key (edge)
	bool xPressed = false; // X key (edge)
	bool levelPrevPressed = false; // L edge (debug)
	bool levelNextPressed = false; // R edge (debug)
};

// Initialise graphics (citro2d) and load embedded sprite sheets (.t3x via headers)
bool hw_init();
void hw_shutdown();

// Input polling (stylus + D-Pad Up)
void hw_poll_input(InputState& out);

// Frame lifecycle (bottom screen only)
void hw_begin_frame();
void hw_end_frame();

// Drawing helpers
void hw_draw_sprite(C2D_Image img, float x, float y, float z=0.0f, float sx=1.0f, float sy=1.0f);

// Access an image from the default (IMAGE) sprite sheet by atlas index
C2D_Image hw_image(int index);

// Simple debug logging to top-screen text console (no-op if console not initialised)
void hw_log(const char* msg);

// Additional sprite sheets (background / UI). All are optional; check loaded before use.
enum class HwSheet : uint8_t { Image, Break, Title, High, Instruct, Designer, Touch, Options };
bool hw_sheet_loaded(HwSheet sheet);
C2D_Image hw_image_from(HwSheet sheet, int index); // returns empty image if missing

// Minimal 5x6 debug font rendering on bottom screen for HUD
void hw_draw_text(int x,int y,const char* text, uint32_t rgba = 0xC8C8C8FF);
void hw_draw_text_scaled(int x,int y,const char* text, uint32_t rgba, float scale);

// Draw recent log lines into current target starting at (x,y); maxPixelsY caps height (optional).
void hw_draw_logs(int x,int y,int maxPixelsY=240);

// Switch current drawing target (top or bottom screen)
void hw_set_top();
void hw_set_bottom();

#endif // PLATFORM_3DS

// Bridge declarations (legacy logic still in main.cpp). These will be refactored.
int leveldesigner(int start_level);
