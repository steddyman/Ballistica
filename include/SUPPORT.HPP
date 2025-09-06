#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#ifdef __cplusplus
#define __CPPARGS ...
#else
#define __CPPARGS
#endif

// Enable runtime collision debug overlays by defining DEBUG as 1.
// To activate, uncomment the next line:
#define DEBUG 1

// 3DS / modern compilers do not use memory model qualifiers; alias 'far' to nothing.
#ifndef far
#define far
#endif


// Setup global definitions

// old handler is a pointer to a function used for int routines
const int TimerInt = 0x1c; // legacy timer interrupt (unused on 3DS)
#ifndef PLATFORM_3DS
char far * const SCRPTR = (char far *) 0xa0000000L; // DOS screen segment
extern char far *maskbuffer;
extern char far *ballbuffer;
#else
// 3DS build uses modern pointers; buffers allocated differently or removed.
extern unsigned char* maskbuffer;
extern unsigned char* ballbuffer;
#endif
extern int DisplayedPage, NonDisplayedPage;
const unsigned char BALLID=200;
const unsigned char BARRIERID=199;
const unsigned char LASERID=198;
const int MAXNAMESIZE=22;	// Maximum size of level name string
const int NAMECOLOUR=3;
const int NAMEEDITCOLOUR=11;
const int SPRPAGE=2;
const int BACKPAGE=3;
const int StepsPixel=128;	// number of steps a pixel
const int NegStepsPixel=0-StepsPixel;
const int EXPDEL=3;			// exploding delay
const int FAKEANI=10;	// fake animation frame to indicate start of explosion
const int MAXFRAME=5;	// maximum explosion frame number
const int MAXSPEED=50;       // maximum ball speed  5 pixels
const int MINSPEED=10;		// minimum ball speed 1 pixel
const int DEFAULTSPEED=20;	// default level start speed
const int SCRLEN = 320;		// Screen length in bytes
const int DEFHEIGHT=9;		// Default block height
const int DEFWIDTH=16;		// Default block width
const int BALLWIDTH=3;		// Default ball width
const int BALLHEIGHT=3;		// Default ball height
const int BATWIDTH=25;          // Width of Bat
const int BATHEIGHT=5;		// Height of Bat
const int CHARXSIZE=8;		// default width of a character
const int LETWIDTH=10;		// default letter width (bonus only)
const int LETHEIGHT=11;		// default letter height (bonus only)
const int CHARYSIZE=8;		// default height of a character
const int BUTTON1=0x01;		// Left mouse button mask
const int BUTTON2=0x02;		// Right mouse button mask
const int SHADOW=1;
const int NOSHADOW=0;
const int RIGHTBORDER=251;
const int LEFTBORDER=10;
const int NUMSIDES=4;
const int TOP=1;
const int RIG=2;
const int BOT=3;
const int LEF=4;
const int DIA=5;
const int UR=5;
const int LR=6;
const int LL=7;
const int UL=8;

// function prototypes

void init_floattable(void);	// setup floating point table

// Setup an enum so names can be used for brick types and above array index.
// These enums are also used for screen level arrays
enum {
	NB, // No Brick
	YB,	// Yellow brick
	GB,	// Green brick
	CB,	// Cyan brick
	TB,	// Tan brick
	PB,	// Purple brick
	RB,	// Red brick
	LB, // Extra Life brick
	SB,	// Slower brick
	FB, // Faster brick
	F1, // Fall slow brick
	F2,	// Fall fast brick
	B1, // B brick
	B2,	// O brick
	B3,	// N brick
	B4,	// U brick
	B5,	// S brick
	BS,	// Bat smaller
	BB,	// Bat bigger
	ID, // indestructible brick
	RW,	// Level Rewind brick
	RE,	// Controls reverse brick
	IS, // Invisible slow brick
	IF,	// Invisible fast brick
	AB,	// Another ball
	FO,	// Level Forward brick
	LA,	// Laser brick
	MB,	// Murderball brick
	BA,	// Bonus brick
	T5,	// five hit brick
	BO,	// bomb brick
	OF,	// off swith brick
	ON, // on switch brick
	SS,	// sideways move slow brick
	SF,	// sideways move fast brick
	EB	// Indicates end of brick list
	};
// class definitions

class ballblock;		// forward reference for collision func
class block			// General block definition
{
protected:
	int     height;                 // height of block
	int     width;                  // width of block
unsigned char *sprdata; // legacy sprite data pointer (PCX) – placeholder for index into atlas
	int     del;                	// status of block
	int		moves;					// indicates block is moving
	int		type;					// type of brick
	int		id;						// id of the brick
	int		sidedir;				// sideways direction
	int		hits;					// number of hits left to destroy
	int		falldel;				// fall delay routine
	int		destruct;				// indestructible brick flag
	int		lasthit;
	int		lasthitx, lasthity;
	int 	solid;					// indicates if brick can pass thru
	int		around[NUMSIDES];		// array of surrounding block nums
//	int		truedelete[2];
	int 	oldx, oldy;		// old x and y co-ordinates for each plane
	int		ox, oy;
	int		srcx, srcy;				// source of image in PAGE2
	int		killed;					// flag to indicate if killed yet
	int		exploding;				// indicates if already exploding
public:
	int x,y;
	int	explodenum;					// block explode number
	// constructor for height width setup and destructor
	block(int w=DEFWIDTH, int h=DEFHEIGHT);
	~block();
	int process_hit(void);	// Bat behaviour routine
	int process_explosion(void);	// Used for bomb destroying bricks
	int is_exploding(void) {return exploding;}
	void not_exploding(void) {exploding=0;}
//	int displayed(int n) {return truedelete[n];}
	virtual void settype(int tp, unsigned char far *pointer);	// setup pointer to sprite data
	void modsize(int w, int h) {width=w; height=h;}
	int gettype(void) {return type;}	// get type of block
	void setid(int i) {id=i;}		// set id of the brick
	virtual void draw();                    // draw block on screen
	int indestructible(void) {return destruct;}	// indestructible check
	// position block on the screen
	virtual void position(int sx, int sy)
		{x=oldx=ox=sx;y=oldy=oy=sy;moves=0;}
	int getx(void) {return x;}	// get x co-ordinate
	int gety(void) {return y;}	// get y co-ordinate
	int getoldx(void) {return oldx;}
	int getoldy(void) {return oldy;}
	int getwidth(void) {return width;} // get width of block
	void resize(int w, int h, unsigned char far *pointer);	// resize block
	int issolid(void) {return solid;}
	// check for a collision between ball and block.  Returns side no or 0
	int collision(const ballblock &ball);
	// check for a collision between ball and bat (1 if true)
	int collision_bat(const ballblock &ball);
	int collision_mouse(const block &mouse);
	int collision(const block &blck);
	int moving_hit(const ballblock &ball);
	virtual void erase();                   // erase block from screen
	virtual void erasemask();
	void droperasemask();
	virtual void drawmask();
	void drawback();
	void save(char *); // legacy (no-op on 3DS)
	void restore(char *);
	void dele();
	void undelete() {if(type!=NB) del=killed=0;}	// mark brick undeleted
//	void settruedel(int n) {truedelete[n]=1;}	// set true delet flag
//	int truedeleted() {return (truedelete[0] & truedelete[1]);}
//	int truedrawn() {return (!truedelete[0] & !truedelete[1]);}
	int deleted() {return del;} // check if block deleted
	int move();		// Move down a line
	int moved() {return (!((oldx==x) && (oldy==y)));}
	int dead() {return killed;}
	void setside(int s, int v) {around[s]=v;}	// set value of side
	int getside(int s) {return around[s];}	// get value of a side
	void setdead(int d) {killed=d;}
	void markmove();	// mark as moving
	int moving() {return moves;} // check if block moving down screen
	void moveside();	// move a brick sideways if necessary
	virtual int getdir(void) {return sidedir;}	// get sideways direction
	void sethit(void) {lasthit=1;}
	void clearhit(void) {lasthit=0;}
	int gethit(void) {return lasthit;}
	virtual void setdir(int d) {sidedir=d;}	// set direction
	int getdelay(void) {return falldel;}	// return fall delay value
	void setdelay(void) {falldel=EXPDEL;}		// set fall delay routine
	void decdelay(void) {falldel--;}			// decrement fall counter
	void startexplosion(void) {explodenum=FAKEANI; falldel=EXPDEL;} // start explosion
};

class ballblock : public block
{
protected:
	unsigned int	colx, coly;	// multiplied versions of co-oridantes x, y
	int	ang;		// Current angle
	int magx;		// Last magnitude setting in x direction
	int magy;		// Last magnitude setting in y direction
	int	spin;		// Spin angle on ball
	int	speed;		// speed of ball
public:
	int	xdir, ydir;	// X/Y direction heading
	// constructor for height width setup
	ballblock(int w=BALLWIDTH, int h=BALLHEIGHT);
	void position(int sx, int sy);
	void sitonbat(block *bat);
	virtual void settype(int tp, unsigned char far *pointer);	// setup pointer to sprite data
	int getspeed(void) {return speed;}
	void setspeed(int s) {speed=s;}
	void incspeed(void) {if(speed<MAXSPEED) speed++;}
	void decspeed(void) {if(speed>MINSPEED) speed--;}
	virtual int getdir(void) {return ang;} // Return current heading
	virtual void setdir(int d);	// set ball direction
	void setspin(int s) {spin=s;}
	void simplecol(int side);	// Simple collision routine on side
	virtual void erasemask();
	virtual void drawmask();
	virtual void draw();
	virtual void erase();
	void batcol(int s);		// Bat collision routine
	void updatepos(int a);	// Update position based on dirx/y
};

// Now define an integer array class for managing the list of moving
// blocks on the screen
class intarray
{
private:
	int     numentries;		// Number of entries currently in array
	int		ceiling;		// Maximum entry number in array
	int		iterator;		// current point in array
	int		*buffer;		// buffer for access to integers
public:
	intarray(int n);		// constructor to allocate memory
	~intarray();			// destructore for deallocating memory
	void add(int v);		// add value v add end of array
	void remove(int n);		// remove element n from the array
	void clear(void) {numentries=0;}		// clear array
	int average(void);		// return average value of array
	int highest(int limit);	// return the highest value in array upto limit
	int value(int n) {return buffer[n];}   // get the current value at n
	int entries(void) {return numentries;} // return current number
										   // of entries in array
};

// TEXT class defines a class that can save information underneath
// itself, draw text to the screen and restore information when erased.
// class also supports shadows for the font, ROM fonts and user defined
// fonts.
class text
{
private:
	int	x, y;         		// current text start location
	int height, width;		// current text height and width
	int shadow;				// shadow on flag
	char *textdata;			// textdata points to current string
	int textlen;			// length of text in characters
	int colour;				// current text colour
	char *background; // legacy background buffer
public:
	text(char *string, int sflag); // constructor
	~text() {delete background; delete textdata;}  // destructor
	// position text on screen
	void position(int sx, int sy) {x=sx; y=sy;}
	void setstring(char *string); // change current text string
	char *getstring(void) {return textdata;} // get current contents of string
	// change current colour
	void setcolour(int col) {colour=col;}
	void erase(void);		// erase current text
	void draw(void);		// draw current text
};

int above(int);
int below(int);
int toleft(int);
int toright(int);
