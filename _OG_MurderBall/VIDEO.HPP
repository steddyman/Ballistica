/*
	GRAPHICS FUNCTIONS FOR GAME HEADER FILE
	---------------------------------------

	Creation Date:	24/01/93
	Author:		Stephen Eddy

	Revision History:


--------------------------------------------------------------------------
*/

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

class PCX {			// Structure for a PCX file
public:
	char far 	*ImageData;	// Pointer to image in memory
	PalData		*Palette;	// Palette for image
	PCX(void) {Palette = new PalData;} // Allocate mem for Palette
	~PCX() {delete Palette;}         	// Free palette memory
	};

// Definition of second dimension of palette structure
//const unsigned char Red = 0;
//const unsigned char Green = 1;
//const unsigned char Blue = 2;
enum {
	PAGE0,
	PAGE1,
	PAGE2,
	PAGE3 };

// Hardware register and segment definitions
static unsigned char VideoSegment = 0xa000;
const int DacWrite = 0x03c8;	// DacWrite register
const int DacRead = 0x03c7;	// DacRead register
const int DacData = 0x03c9;	// DacData register
const int InputStatus = 0x03da;    // Input status register
const char VbiBit = 0x8;   // Bit for vertical retrace interupt

// Function prototypes for assembly module
extern "C" {
void fillOffsets(void);
void blitSprite(int, int, int, int, char far *);
void blitBrick(int, int, char far *);
void blitBrickBack(int, int, char far *, char far *);
void eraseBrick(int, int, char far *);
void eraseSprite(int, int, int, int, char far *);
void blitBall(int, int, char far *);
void eraseBall(int, int, char far *);
void xblitSprite(int, int, int, int, int, int, int, int);
void xeraseSprite(int, int, int, int, int, char far *);
void xcopyPage(int, int);
void blitMask(int, int, int, int, int, char far *);
int getMaskPixel(int, int);
void xPutImage(char far *, int);
void xSetPage(int);
int testMaskLine(int, int);
void setMaskPixel(int, int, int);
}

// Function Prototypes
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
void DisplayImage(char far *, char far *);
void DisplayImageX(char far *, int);
PCX *ReadImage(char *);
void GraphicsMode(void);
void TextMode(void);
void SetColour(char, char, char, char);
void WaitRetrace(void);
void DrawSprite(int, int, void far *);
void SaveBack(int, int, void far *);
void RestoreBack(int, int, void far *);
void ReadJoystick(int);
void ROMFont(void);
void DrawText(char far *, int, int, char);
void ClearBox(int, int, int, int, char);
void ClearScreen(void);
char ReadFire(int);
uint ReadPot(char);