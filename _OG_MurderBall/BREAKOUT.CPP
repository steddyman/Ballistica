#include <iostream.h>
#include <fstream.h>
#include <conio.h>
#include <bios.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include "support.hpp"
#include "video.hpp"
#include "sprcode.cpp"	// sprite code for graphics

const int KEY_F1=0x3b00;
const int KEY_F2=0x3c00;
const int KEY_F3=0x3d00;
const int KEY_F4=0x3e00;

const int BricksX=13;
const int BricksY=11;
const int NumBricks = BricksX * BricksY;	// number of bricks
const int NORMBALL=40;
const int MURDERBALL=50;
const int MLIMIT=35;		// max movement mouse limit
const int LEFTSTART=28;
const int TOPSTART=18;
const int RIGHTEND=LEFTSTART+BricksX*DEFWIDTH;
const int BOTTOMEND=TOPSTART+BricksY*DEFHEIGHT;
const int REVWIDTH=25;
const int REVHEIGHT=2;
const int LASERWIDTH=3;
const int LASERHEIGHT=6;
const int BATCOUNT=500;
const int REVSTEP=BATCOUNT/REVWIDTH;
const int REVX=275;
const int REVY=131;
const int BATY=185;
const int SCRBOT=192;
const int SelectColumnX=260;	// Position of selection columns for brick
const int SelectColumnY=52;
const int DrawBrickX=124;
const int DrawBrickY=142;
const int LevelTextX=199;
const int LevelTextY=170; // was 188
const int SpeedTextX=199;	// Speed indicator for level indicator
const int SpeedTextY=185;
const int SpeedMinusX=139;
const int SpeedMinusY=184;
const int SpeedPlusX=218;
const int SpeedPlusY=184;
const int EffectTextX=75;
const int EffectTextY=153;
const int ClearButtonX=108;
const int ClearButtonY=169;
const int ClearSizeX=21;
const int ClearSizeY=9;
const int ExitButtonX=59;
const int ExitButtonY=184;
const int ExitSizeX=18;
const int ExitSizeY=9;
const int PlusSignX=218;
const int PlusSignY=169;
const int MouseSize=4;
const int LevelChangeSizeX=10;
const int LevelChangeSizeY=9;
const int MinusSignX=139;
const int MinusSignY=169;
const int NameButtonX=28;
const int NameButtonY=7;
const int NameButtonSizeX=22;
const int NameButtonSizeY=9;
const int NameTextX=56;
const int NameTextY=7;
const int LevelTestX=196;
const int LevelTestY=141;
const int LevelTestSizeX=40;
const int LevelTestSizeY=9;
const int TITLECOLOUR=90;	// colour number for high score title
const int NORMCOLOUR=27;	// normal colour of high score entries
const int HIGHCOLOUR=3;		// editing colour of high score entries
const int TITLEXPOS=10;
const int TITLEYPOS=50;
const int SCOREXPOS=10;
const int SCOREYPOS=70;
const int SCOREYDIFF=14;
const int EDITCOL=130;
const int HIGHSCRNUM=2;		// sequence number of high score screen
const int LOOPREPS=2000;
const int MAXLEVEL=50;	// absolute maximum level number
const int NUMSCORES=8;	// number of high score entries
const int LIFEGAP=5;	// gap between life graphic indicators
const int LIFEHEIGHT=3;	// height of above
const int NumTypes=6;	// different no of normal type bricks
const char White = 15;
const char BlueC = 1;
const int CURLEFT = 0x4b00;	// Left key
const int CURRIGHT = 0x4d00;	// right key
const int ESCAPE = 0x011b;	// escape key
const int MOMMAX = 5;		// backtrack of last five speeds
const int DELMAX = 10;		// maximum number of simultaneous deleted blocks
const int StartColour=32;	// Offset to first colour for brick 0
const int WAITBAT=10;		// bat and ball movement delay
const int TopBorderX=5;
const int TopBorderY=0;
const int LeftBorderX=5;
const int LeftBorderY=5;
const int RightBorderX=251;
const int RightBorderY=5;
const int BorderWidth=5;
const int LeftBorderEndX=LeftBorderX+BorderWidth;
const int TopBorderEndY=TopBorderY+BorderWidth;


extern float PiOver180;
extern int CosDir[362];
extern int SinDir[362];
extern unsigned int PageOffsets[];
extern int mrange;	// mouse movement range for collisions

// Setup list of pointers to bricks in an array
unsigned char far *brick_pointers[] = {
	NULL,
	yellow_brick,
	green_brick,
	cyan_brick,
	tan_brick,
	purple_brick,
	red_brick,
	life_brick,
	slow_brick,
	fast_brick,
	skull_brick,
	skull_brick,
	b_brick,
	o_brick,
	n_brick,
	u_brick,
	s_brick,
	batsmall_brick,
	batbig_brick,
	indestructible_brick,
	rewind_brick,
	reverse_brick,
	islow_brick,
	ifast_brick,
	another_ball,
	forward_brick,
	laser_brick,
	murderball_brick,
	bonus_brick,
	fivehit_brick,
	bomb_brick,
	offswitch_brick,
	onswitch_brick,
	sideslow_brick,
	sidehard_brick
	};

// shorthand brick name array
char far *brick_shorthand[] = {
	"NB",	// No Brick
	"YB",	// Yellow brick
	"GB",	// Green brick
	"CB",	// Cyan brick
	"TB",	// Tan brick
	"PB",	// Purple brick
	"RB",	// Red brick
	"LB", 	// Extra Life brick
	"SB",	// Slower brick
	"FB", 	// Faster brick
	"F1", 	// Fall slow brick
	"F2",	// Fall fast brick
	"B1", 	// B brick
	"B2",	// O brick
	"B3",	// N brick
	"B4",	// U brick
	"B5",	// S brick
	"BS",	// Bat smaller
	"BB",	// Bat bigger
	"ID", 	// indestructible brick
	"RW",	// Level Rewind brick
	"RE",	// Controls reverse brick
	"IS", 	// Invisible slow brick
	"IF",	// Invisble fast brick
	"AB",	// Create another ball
	"FO",	// Level Forward brick
	"LA",	// Laser brick
	"MB",	// MurderBall brick
	"BA",	// Bonus brick
	"T5",	// five hit brick
	"BO",	// bomb brick
	"OF",	// off swith brick
	"ON",	// on switch brick
	"SS",	// sideways move slow brick
	"SF",	// sideways move indestructible brick
	NULL
	};

// full name for brick array
char far *brick_fullname[] = {
	"No brick",
	"10 Points",
	"20 Points",
	"30 Points",
	"40 Points",
	"50 Points",
	"100 Points",
	"Gives extra life",
	"Slows down ball",
	"Speeds up ball",
	"Destroys bat, slow",
	"Destroys bat, fast",
	"Letter B of bonus",
	"Letter O of bonus",
	"Letter N of bonus",
	"Letter U of bonus",
	"Letter S of bonus",
	"Makes bat smaller",
	"Makes bat bigger",
	"Indestructible brick",
	"Rewinds level",
	"Reverse bat controls",
	"Slow down, immediate",
	"Speed up, immediate",
	"Add another ball",
	"Forwards level",
	"LaserBat",
	"MurderBall",
	"1000 points bonus",
	"5 hits to destroy",
	"Ignites a bomb",
	"Turn lights off",
	"Turn lights on",
	"Move sideways slowly",
	"Move sideways, Indest."
	};

char far *highscore_names[NUMSCORES];	// Array for higscore names

char far *level_data[MAXLEVEL+1];	// pointer to info for levels
char far *level_name[MAXLEVEL+1];	// pointer to names for levels
int level_speed[MAXLEVEL+1];		// allocate array for level speeds
char far *backptr;					// global background pointer
ballblock *balls[NumBricks];			// allocate an array for a maximum number of ball
block *bricks[NumBricks];
unsigned long high_score[NUMSCORES];	// high score table
unsigned char high_level[NUMSCORES];	// Level reached
char far high_name[NUMSCORES][MAXNAMESIZE+1];		// names of players
char far *picture_sequence[] = {	// title page sequence
	"title.pcx",
	"instruct.pcx",
	"high.pcx",
	NULL
	};

// list of pointers to bonus indicator letters
unsigned char far *letter_pointers[] = {
	letterb,
	lettero,
	lettern,
	letteru,
	letters
	};

// Pointer to sprites for displaying lives indicator
unsigned char far *life_graphics[] = {
	NULL,
	lives_1,
	lives_2,
	lives_3,
	lives_more
	};

// Pointer to sprites for displaying exploding brick animation
unsigned char far *exploding_graphics[] = {
	exp1_brick,
	exp2_brick,
	exp3_brick,
	exp4_brick,
	exp5_brick,
	exp6_brick,
	exp7_brick
	};

int DisplayedPage,  NonDisplayedPage;

char far *levelfile = "levels.dat";	// name of level input file
char far *maskbuffer;	// pointer of offscreen mask buffer
char far *ballbuffer;	// pointer for offscreen ball mask buffer

int maingame(int);		// main breakout game routine
int leveldesigner(int);	// level designer section
int bioskey(int);		// Bioskey routine to read a scan code
int create_level(int);	// Routine to fill in the brick structure
void editstring(text *);	// string object editing routine
void blocks_to_array(int);	// move blocks array into level array
int read_levels(void);	// Routine to read in all levels data
int write_levels(void);	// rotuine to write out entire level array
int count_required(void);	// count number of blocks required to complete level
int position_bat(block &);	// routine to position bat
int add_ball(int, int);	// routine to add new ball into array
int add_newball(int, int, int, int);	// as above for when hit with laser
int brickat(int, int);
void allocatebricks(void);
void allocateballs(void);
void allocatelevels(void);
void deallocatelevels(void);
void deallocatebricks(void);
void deallocateballs(void);
void draw_lives(block *, int);	// Draw lives left on panel
void draw_revbar(block *, int);	// Draw amount of reverse bar left
void setup_sides(void);			// fill in the around[] array in bricks
void cleararrays(void);
int check_highscore(unsigned long, int); // check and insert score entry
int read_highscores(void);
int write_highscores(void);
void display_highscores(int);		// display high score table
int above(int);
int below(int);
int toleft(int);
int toright(int);


// global variables
int batreverse;		// flag to indicate bat movement reversed
int oldrevwidth=BATCOUNT*2;	// set to impossible to reach value
int toplevel=0;
int numballs;
int oldnumballs;
unsigned long lastscore;	// last score acheived
int lastlevel;
int exitnormal;			// flag to indicate game completed normally

int main(void)
{
	int exitmain=FALSE;
	int key;
	PCX *Screen;
	int start_level=0;
	int changeflag=0;
	int scrnum=0;		// currently displayed screen number
	int highpos;
	int countdown;
	unsigned int mmx, mmy, thr;
	ROMFont();
	if(MouseReset()!=-1) {
		cout << "\nFailed to find a Microsoft compatible mouse.\n";
		cout << "Exiting program.\n";
		return 1;
	}
	randomize();		// seed random number generator
	cleararrays();		// clear level, name and speed arrays
	init_floattable();
	SetVMode(0x13);
	fillOffsets();		// fill in screen start address line array
	asm {
		mov ax,1bh
		int 33h
		mov mmx, bx
		mov mmy,cx
		mov thr,dx
		mov bx,28
		mov cx,28
		mov dx,28
		mov ax,1ah
		int 33h
	}
	DisplayedPage=0;
	NonDisplayedPage=1;
	read_highscores();
	highpos=-1;
	mrange=30;
	while(!exitmain) {	// loop until told to exit
		ClearPalette();		// clear palette to black
		Screen=ReadImage(picture_sequence[scrnum]); // get current screen
		DisplayImage(Screen->ImageData, 0);	// display current screen
		if(scrnum==HIGHSCRNUM) display_highscores(-1); // the scores on the doors
		Fade2Palette(Screen->Palette);	// and fade palette in
		countdown=LOOPREPS;
		if(highpos!=-1) { // if edit high score situation
			backptr=new char[64000L]; // allocate memory for backptr
			DisplayImage(Screen->ImageData, backptr); // draw screen into background
			text newname(high_name[highpos], NOSHADOW);
			newname.position(EDITCOL, SCOREYPOS+(highpos*SCOREYDIFF));
			newname.setcolour(HIGHCOLOUR);
			newname.draw();
			editstring(&newname);	// edit string on screen
			strcpy(high_name[highpos], newname.getstring()); // copy new name
			newname.erase();
			newname.setcolour(NORMCOLOUR);
			newname.draw();
			delete backptr;
			delay(1000);	// delay 1 second
			Fade2Black(Screen->Palette);
			delete Screen->ImageData; // delete screen memory
			delete Screen;
			scrnum=0;	// reset to title screen
			highpos=-1;	// cancel high score edit mode
			continue;	// back to out while loop
		}
		while(countdown) {
			if(changeflag || bioskey(1)) {	// if a keypressed
				if(!changeflag) key=bioskey(0);	// get key code
				switch(key) {
				  case KEY_F1:	// New game key hit
					if(!changeflag) Fade2Black(Screen->Palette);
					maingame(start_level);	// launch main game
					if(start_level) { // if in test mode
						key=KEY_F2;
//						ClearPalette();
						break;
					}
					if(exitnormal) { // if game completed normally
						highpos=check_highscore(lastscore, lastlevel);
					} else highpos=-1;
					countdown=0;	// exit loop to reload
					scrnum=-1;
					break;
				  case KEY_F2:	// Designer key pressed
					if(!changeflag) Fade2Black(Screen->Palette);
					start_level=leveldesigner(start_level);	// goto level designer
					if(start_level) {
						changeflag=1;
						key=KEY_F1;
//						ClearPalette();
						break;	// back around switch
					} else changeflag=0;
					countdown=0;		// exit loop
					scrnum=-1;
					break;
				  case KEY_F4:	// exit game key
				  case ESCAPE:
					countdown=0;;
					exitmain=TRUE;
					break;
				  default:	// another key, reset to title screen
					if(scrnum) {	// if not already 0
						scrnum=-1;	// title screen number
						countdown=0;	// exit loop time
					}
					break;
				}
			}
			delay(5);
			if(countdown) countdown--;	// reduce countdown
		}
		if(scrnum!=-1) Fade2Black(Screen->Palette);
		delete Screen->ImageData;
		delete Screen;
		scrnum++;
		if(picture_sequence[scrnum]==NULL) scrnum=0; // restart at end of sequence
		if(highpos!=-1) scrnum=HIGHSCRNUM; // if displaying high scores
	}
	write_highscores();		// write high scores to disk
	SetVMode(0x03);		// mode back to text mode
	cout << "Thanks for playing MurderBall." << endl << endl;
	return 0;
}

int leveldesigner(int start_level)
{
	PCX *Designer;
	int i,t, n;		// temporary loop variables
	int level, lastbrick, speed;
	int mx, my, mb;
	int x, y;
	int finished, hit;
	int cbrick=YB;	// current brick
	block *selections[EB];
	char temp[80];
	char name[MAXNAMESIZE+1];	// level name plus the zero

	name[0]='\0';

	Designer=ReadImage("designer.pcx");
	if(Designer==NULL) {
		errhandler("Error opening level designer graphics.", 1);
	}
	backptr=new char[64000];			// allocate memory for back image
	if(backptr==NULL) {
		errhandler("No memory for background image.", 2);
	}
	DisplayImage(Designer->ImageData, backptr);	// setup pointer to background
	allocatelevels();
	toplevel=read_levels();		// read in levels structure
	level=1; //start at level 1
	if(start_level) level=start_level;  // unless told otherwise
	speed=DEFAULTSPEED;
	allocatebricks();
	ClearPalette();		// Palette to black
	DisplayImage(Designer->ImageData,0);
	n=0;
	for(i=0; i<BricksY; i++) {
		for(t=0; t<BricksX; t++) {
			bricks[n]->position((t*(DEFWIDTH))+28,(i*(DEFHEIGHT))+18);
			n++;
		}
	}
	for(i=1; i<EB; i++) {
		selections[i]=new block;
		selections[i]->settype(i, brick_pointers[i]);
	}
	selections[0]=new block;
	selections[0]->settype(50, designer_nobrick);
	// first draw left column of selection bricks
	n=SelectColumnY; t=SelectColumnX; i=0;
	while(i<EB) {
		if(n+DEFHEIGHT>=SCRDEPTH) {
			n=SelectColumnY;
			t+=DEFWIDTH+3;
		}
		selections[i]->position(t, n); // Position left column
		selections[i]->drawback();
		selections[i]->draw();
		n+=DEFHEIGHT+3;
		i++;
	}
	// now create brick to show current drawing brick and position and draw it
	block drawbrick;
	drawbrick.position(DrawBrickX, DrawBrickY);
	drawbrick.settype(cbrick, brick_pointers[cbrick]);	// default brick is yellow
	drawbrick.draw();


	// create the plus and minus signs for level increase/decrease
	block plussign(LevelChangeSizeX, LevelChangeSizeY);
	block minussign(LevelChangeSizeX, LevelChangeSizeY);
	plussign.position(PlusSignX, PlusSignY);
	minussign.position(MinusSignX, MinusSignY);
	plussign.settype(101, button_plus);
	minussign.settype(102, button_minus);
	plussign.draw();
	minussign.draw();

	// Nov create plus/minus for speed increase/decrease
	block speedplus(LevelChangeSizeX, LevelChangeSizeY);
	block speedminus(LevelChangeSizeX, LevelChangeSizeY);
	speedplus.position(SpeedPlusX, SpeedPlusY);
	speedminus.position(SpeedMinusX, SpeedMinusY);
	speedplus.settype(101, button_plus);
	speedminus.settype(102, button_minus);
	speedplus.draw();
	speedminus.draw();

	// Now design level name button
	block namebutton(NameButtonSizeX, NameButtonSizeY);
	namebutton.position(NameButtonX, NameButtonY);
	namebutton.settype(105, button_levelname);
	namebutton.draw();	// draw button

	// Now activate the clear level button
	block clearlevel(ClearSizeX, ClearSizeY);
	clearlevel.position(ClearButtonX, ClearButtonY);
	clearlevel.settype(103, button_clear);
	clearlevel.draw();

	// Now activate the exit button
	block exitdesigner(ExitSizeX, ExitSizeY);
	exitdesigner.position(ExitButtonX, ExitButtonY);
	exitdesigner.settype(104, button_exit);
	exitdesigner.draw();

	// Now test level button
	block leveltest(LevelTestSizeX, LevelTestSizeY);
	leveltest.position(LevelTestX, LevelTestY);
	leveltest.settype(105, test_level);
	leveltest.draw();

	create_level(level);	// read in current level to structure
	speed = level_speed[level]; // read the speed of the current level
	for(n=0;n<NumBricks;n++) {
		bricks[n]->draw();
	}

	// create text for level number
	sprintf(temp, "%02d", level);
	text leveltext(temp, NOSHADOW);	// create text for level
	leveltext.position(LevelTextX, LevelTextY);	// position the text
	leveltext.setcolour(15);
	leveltext.draw();

	// create text for level speed
	sprintf(temp, "%02d", speed);
	text speedtext(temp, NOSHADOW); // create text for speed
	speedtext.position(SpeedTextX, SpeedTextY);	// position text
	speedtext.setcolour(15);	// colour to white
	speedtext.draw();

	// create text for level name
	strcpy(name, level_name[level]);
	text lname(name, NOSHADOW);	// create text for level name
	lname.position(NameTextX, NameTextY);
	lname.setcolour(NAMECOLOUR);	// colour to red
	lname.draw();		// draw level name

	text effecttext(brick_fullname[cbrick], NOSHADOW);
	effecttext.position(EffectTextX, EffectTextY);
	effecttext.setcolour(7);
	effecttext.draw();

	Fade2Palette(Designer->Palette);
	// now create the mouse cursor sprite
	char far *mouseback = new char[MouseSize*MouseSize]; // allocate mem for screen save
	block mouse(MouseSize, MouseSize);
	mouse.position(SCRWIDTH/2,SCRDEPTH/2);
	x=SCRWIDTH/2;
	y=SCRDEPTH/2;
	mouse.settype(100, mouse_cursor);
	mouse.save(mouseback);
	mouse.draw();
	lastbrick=-1;
	finished=0;
	while(!finished) {
		WaitRetrace();	// wait for screen retrace
		mb=ReadMouse(mx, my);	// read mouse status
		if(mx>0) {	// move horizontally to right
			if(x+mx<SCRWIDTH-10) x+=mx;
			else x=SCRWIDTH-10;
		} else if(mx<0) { // move horizontally to left
			if(x+mx>10) x+=mx;
			else x=10;
		}
		if(my>0) {	// move vertically down
			if(y+my<SCRDEPTH-5) y+=my;
			else y=SCRDEPTH-5;
		} else if(my<0) {	// move vertically up
			if(y+my>5) y+=my;
			else y=5;
		}
		if((mb & BUTTON1)==BUTTON1) {	// left button pressed
			if(x>=SelectColumnX) {	// if on right of screen check array
				for(i=0;i<EB; i++) {	// loop for types on right
					hit=selections[i]->collision_mouse(mouse); // over brick
					if(hit) {	// if over a selection block
						// then erase old, set type and redraw
						drawbrick.erase();	// erase current drawing block
						effecttext.erase();
						if(i) {
							cbrick=selections[i]->gettype();
							drawbrick.resize(DEFWIDTH, DEFHEIGHT, brick_pointers[cbrick]);
						} else cbrick=0;
						drawbrick.settype(cbrick, brick_pointers[cbrick]);
						effecttext.setstring(brick_fullname[cbrick]);
						effecttext.draw();
						drawbrick.draw();
						while(MouseButton() & BUTTON1) ;// wait for release
						break;
					}
				} // else check if over the brick array
			} else if((x>=LEFTSTART && x<RIGHTEND) && (y>=TOPSTART && y<BOTTOMEND)) {
				// n = brick number cursor is over
				n=(((y-TOPSTART)/DEFHEIGHT)*BricksX)+((x-LEFTSTART)/DEFWIDTH);
				if(n==lastbrick) continue;	// don't draw twice
				cbrick=drawbrick.gettype();	// get current drawing brick
				mouse.restore(mouseback);		// erase move
				bricks[n]->erase();	// erase old brick
				bricks[n]->resize(DEFWIDTH, DEFHEIGHT, brick_pointers[cbrick]);
				bricks[n]->settype(cbrick, brick_pointers[cbrick]); // change type
				bricks[n]->draw();
				mouse.save(mouseback);
				mouse.draw();
			} else {
				// check if clear button selected
				if(clearlevel.collision_mouse(mouse)) {
					mouse.restore(mouseback);
					for(i=0; i<NumBricks; i++) {
						bricks[i]->erase();
						bricks[i]->settype(NB, NULL);
					}
					mouse.save(mouseback);
					mouse.draw();
					while(MouseButton() & BUTTON1) ;// wait for release
				} else if(exitdesigner.collision_mouse(mouse)) { // exit button??
					finished=1;	// mark finished
					break;
				} else if(leveltest.collision_mouse(mouse)) { // test level selected
					finished=100+level;
					break;
				} else if(speedminus.collision_mouse(mouse)) { // minus speed??
					if(speed>MINSPEED) { // make sure not to slow
						speed--;	// decrease speed
						mouse.restore(mouseback);
						speedtext.erase();	// now update speed text
						sprintf(temp, "%02d", speed);
						speedtext.setstring(temp);
						speedtext.draw();
						mouse.save(mouseback);
						mouse.draw();
						while(MouseButton() & BUTTON1) ;// wait for release
					}
				} else if(speedplus.collision_mouse(mouse)) { // plus speed??
					if(speed<MAXSPEED) { // make sure not to fast
						speed++;	// increase speed
						mouse.restore(mouseback);
						speedtext.erase();	// now update speed text
						sprintf(temp, "%02d", speed);
						speedtext.setstring(temp);
						speedtext.draw();
						mouse.save(mouseback);
						mouse.draw();
						while(MouseButton() & BUTTON1) ;// wait for release

					}
				} else if(namebutton.collision_mouse(mouse)) {	// change level name?
					while(MouseButton() & BUTTON1) ;	// wait mouse release
					mouse.restore(mouseback);
					editstring(&lname);	// edit name string;
					strcpy(name, lname.getstring()); // get new string
					mouse.save(mouseback);
					mouse.draw();
				} else if(plussign.collision_mouse(mouse)) { // plus level??
					if(level<toplevel) {	// if level to move into
						blocks_to_array(level);	// write into array
						level_speed[level]=speed; // save new speed
						// set new level name
						if(level_name[level]!=NULL) delete level_name[level];
						level_name[level]=new char[strlen(name)+1];
						strcpy(level_name[level], name);
						mouse.restore(mouseback); // earse mouse
						// erase old array from screen
						for(i=0;i<NumBricks;i++) bricks[i]->erase();
						level++;	// increase level
						create_level(level); // fill array with new entries
						speed=level_speed[level]; // get new speed
						strcpy(name, level_name[level]); // get new name
						lname.erase();	// delete old name
						lname.setstring(name);
						lname.draw();
						for(i=0; i<NumBricks;i++) bricks[i]->draw();
						leveltext.erase();
						sprintf(temp, "%02d", level);
						leveltext.setstring(temp);
						leveltext.draw();
						speedtext.erase();	// now update speed text
						sprintf(temp, "%02d", speed);
						speedtext.setstring(temp);
						speedtext.draw();
						mouse.save(mouseback);
						mouse.draw();
						while(MouseButton() & BUTTON1) ;// wait for release
					} else if(level<MAXLEVEL) {	// create a new level time
						blocks_to_array(level);	// write into array
						level_speed[level]=speed; // save new speed
						// set new level name
						if(level_name[level]!=NULL) delete level_name[level];
						level_name[level]=new char[strlen(name)+1];
						strcpy(level_name[level], name);
						mouse.restore(mouseback); // earse mouse
						// erase old array from screen
						for(i=0;i<NumBricks;i++) bricks[i]->erase();
						level++;	// increase level
						speed=DEFAULTSPEED; // new level at default speed
						speed=level_speed[level]; // get new speed
						strcpy(name, level_name[level]); // get new name
						lname.erase();	// delete old name
						lname.setstring(name);
						lname.draw();
						level_data[level]=new char[NumBricks+1]; // allocate memory;
						toplevel++; // increase top level number
						for(i=0;i<NumBricks; i++) bricks[i]->settype(NB, NULL);
						leveltext.erase();
						sprintf(temp, "%02d", level);
						leveltext.setstring(temp);
						leveltext.draw();
						speedtext.erase();	// now update speed text
						sprintf(temp, "%02d", speed);
						speedtext.setstring(temp);
						speedtext.draw();
						mouse.save(mouseback);
						mouse.draw();
						while(MouseButton() & BUTTON1) ;// wait for release
					}

				} else if(minussign.collision_mouse(mouse)) { // minus level
					if(level>1) {
						blocks_to_array(level);	// write into array
						level_speed[level]=speed; // save new speed
						// set new level name
						if(level_name[level]!=NULL) delete level_name[level];
						level_name[level]=new char[strlen(name)+1];
						strcpy(level_name[level], name);
						mouse.restore(mouseback); // earse mouse
						// erase old array from screen
						for(i=0;i<NumBricks;i++) bricks[i]->erase();
						level--;	// idecreasee level
						create_level(level); // fill array with new entries
						speed=level_speed[level]; // get new level speed;
						speed=level_speed[level]; // get new speed
						strcpy(name, level_name[level]); // get new name
						lname.erase();	// delete old name
						lname.setstring(name);
						lname.draw();
						for(i=0; i<NumBricks;i++) bricks[i]->draw();
						leveltext.erase();
						sprintf(temp, "%02d", level);
						leveltext.setstring(temp);
						leveltext.draw();
						speedtext.erase();	// now update speed text
						sprintf(temp, "%02d", speed);
						speedtext.setstring(temp);
						speedtext.draw();
						mouse.restore(mouseback);
						mouse.draw();
						while(MouseButton() & BUTTON1) ;// wait for release
					}
				}
			}
		}
		if((mb & BUTTON2)==BUTTON2) {	// right button pressed
			if((x>=LEFTSTART && x<RIGHTEND) && (y>=TOPSTART && y<BOTTOMEND)) {
				n=(((y-TOPSTART)/DEFHEIGHT)*BricksX)+((x-LEFTSTART)/DEFWIDTH);
				if(n==lastbrick) continue;	// don't draw twice
				cbrick=bricks[n]->gettype();	// get current drawing brick
				mouse.restore(mouseback);		// erase move
				drawbrick.erase();	// erase old brick
				effecttext.erase();
				drawbrick.settype(cbrick, brick_pointers[cbrick]); // change type
				effecttext.setstring(brick_fullname[cbrick]);
				effecttext.draw();
				drawbrick.draw();
				mouse.save(mouseback);
				mouse.draw();
				while(MouseButton() & BUTTON2) ;	// wait for release
			}
		}
		if(mx!=0 || my!=0) {	// mouse moved
			mouse.restore(mouseback);
			mouse.position(x, y);
			mouse.save(mouseback);
			mouse.draw();
		}
	} // end while loop, time to exit
	blocks_to_array(level);	// fill array with bricks
	level_speed[level]=speed; // update last speed value
	strcpy(level_name[level], name);	// update level name
	write_levels();	// write level to disk
	Fade2Black(Designer->Palette);
	deallocatelevels();
	deallocatebricks();
	delete backptr;
	delete Designer->ImageData;	// erase memory for PCX file
	delete Designer;
	// finally delete memory assigned to level array
	for(i=0;i<EB;i++) delete selections[i];
	delete mouseback;
	if(finished>100) return finished-100;	// test level exit
	else return 0;	// normal exit
}




// Main breakout game routine
int maingame(int start_level)
{
	int mousepos;		// temporary position holder
	int bhit;		// bat hit status
	int colstat;	// bat collide status
	int batsize=1;	// default bat size
	int lives=10;		// Number of lives
	unsigned long score=0L;
	unsigned lp;
	unsigned long scorelevel=10L;	// default lowest brick score
	int finished = 0;   // end of loop indicator
	int testmode;		// indicator for level test mode
	int level;
	int key=0;
	int done=0;
	int speed;
	int bonusbits=0;	// bonus bit field
	int loop;
	int batcollide=0;	// flag for collision with bat

	int batcountdown;	// countdown to cancel bat reverse effects
	int maxmoving;		// maximum number of moving bricks
	int mousemove=0;
	int nummoving;
	int numneeded;		// counter for number of bricks required for level comp
	intarray *movearray;	// pointer to array of moving bricks
	PCX *MainGame;
	int cl;				// colision flag
	int b;	   			// ball count loop
	int tx, ty;			// temporary x and y counter
	int pball;			// flag to indicate reposition bat and ball
	int globalspeed=0;	// global speed change flag
	int	lastalive;
	int af, bf, cf;
	int h1, h2, h3;
	int d1, d2;
	int bx, by;
	int dir, olddir;	// current and old directions
	int laserflag=0;		// flag to indicate if laser active
	int laserfired=0;		// flag to indicate if laser fired
	int hitarray[3];
	int lightflag=1;
	int firsttime=1;		// flag to indicate draw level name and number
	char scoretext[7];  // score text
	char leveltext[3];
	char livestext[3];
	char levelnum[20]; // level number text
	char levelname[MAXNAMESIZE+1];// level name text
	PalData *Palette = new PalData;

	// first allocate memory for offscreen buffer
	maskbuffer=new char[64000];
	if(maskbuffer==NULL) {
		errhandler("Out of memory allocating memory for mask buffer.", 3);
	}
	ballbuffer=new char[64000];
	if(ballbuffer==NULL) {
		errhandler("Out of memory allocating memory for mask buffer.", 4);
	}
	if(start_level) {
		level=start_level;
		testmode=TRUE;
	} else {
		level=1;
		testmode=FALSE;
	}
	intarray mousearray(MOMMAX);
	intarray delarray(NumBricks); // setup brick kill arrray with DELMAX entries
	intarray balldelarray(NumBricks);	// array for number of balls to delete
	intarray explodearray(NumBricks);
	movearray=new intarray(NumBricks);	// allocate space for array
	SetupTimer();
	ProgramTimer0(19879);	// set 60hz timer frequemcy
	for(loop=0; loop<MOMMAX; loop++) mousearray.add(0);	// fill array with zeros
	allocatebricks();	// allocate memory for bricks
	allocatelevels();
	block bonus[5];
	block laser(LASERWIDTH, LASERHEIGHT);	// define and setup laser
	laser.settype(109, laser_sprite);
	laser.setid(LASERID);
	block lindicator(3,3);	// setup laser indicator
	lindicator.settype(110, laser_indicator);
	block *lifegraph=new block(3, LIFEHEIGHT);
	lifegraph->position(277, 117);	// position life graphics
	lifegraph->settype(101, life_graphics[4]);
	block *revgraph=new block(REVWIDTH, REVHEIGHT);
	revgraph->position(REVX, REVY);
	revgraph->settype(102, reverse_bar);
	for(loop=0; loop<5; loop++) {
		bonus[loop].position(282, 38+(14*loop));	// position bonus block
		bonus[loop].resize(LETWIDTH, LETHEIGHT, letter_pointers[loop]);
		bonus[loop].settype(50+loop, letter_pointers[loop]); // set type
	}
	toplevel=read_levels();	// read levels into memory array
	if(toplevel==0) {
		errhandler("No levels or level file corrupted.", 4);
	}
	block bat(BATWIDTH, BATHEIGHT);
	bat.settype(50, bat_normal);
	allocateballs();
	balls[0]->settype(NORMBALL, ball_sprite);
	balls[0]->setid(BALLID);
	text lnum("Level: 01", NOSHADOW);	// text object for level number
	text lname("Level name", NOSHADOW);	// text object for level description
	lnum.setcolour(15);
	lname.setcolour(15);
	lnum.position(124, 86);
	MainGame=ReadImage("break.pcx");	// read in screen image
	backptr=new char[64000];			// allocate memory for back image
	if(backptr==NULL) {
		errhandler("No memory for background image", 5);
	}
	if(MainGame==NULL) {
		errhandler("No memory for background image", 6);
	}
	while(!done) {
		numballs=1;		// current number of balls
		laserflag=laserfired=0;
		lightflag=1;
		oldnumballs=numballs;

		for(lp=0;lp<64000;lp++) maskbuffer[lp]=0;	// clear buffer
		for(lp=0;lp<64000;lp++) ballbuffer[lp]=0;	// clear ball buffer
		bat.position((RIGHTBORDER-bat.getwidth())/2,185);
		ClearPalette();		// Palette to black

		DisplayImage(MainGame->ImageData, backptr);	// setup pointer to background
		DisplayImage(MainGame->ImageData, 0);	// display image on screen
		if(firsttime) {	// first time thru loop display level description
			strcpy(levelname, level_name[level]);  // get level name
			lname.position(160-((8*strlen(levelname))/2), 104); // centre name
			lname.setstring(levelname);	// set string to level name
			sprintf(levelnum, "Level: %02d", level);
			lnum.setstring(levelnum);
			lnum.draw();
			lname.draw();
			SetColour(15, 0, 20, 255); // colour 15 to blue
			WaitRetrace();
			delay(8000);
			SetColour(15, 0, 0, 0);
			lname.erase();
			lnum.erase();
			firsttime=0;	// mark so doesn't repeat this loop
		}
		int i,t, n, v;
		n=0;
		for(i=0; i<BricksY; i++) {
			for(t=0; t<BricksX; t++) {
				bricks[n]->position((t*(DEFWIDTH))+28,(i*(DEFHEIGHT))+18);
				n++;
			}
		}
		if(create_level(level)!=level) {
			SetVMode(0x03);
			errhandler("Error in level file.", 7);
		}
		numneeded=count_required(); // count number needed for level
		for(n=0; n<NumBricks; n++) {
			bricks[n]->undelete();
			bricks[n]->not_exploding();
			balls[n]->dele();
			v=bricks[n]->gettype();
			if(v!=NB) bricks[n]->drawmask();
			if(v==SS || v==SF) movearray->add(n);	// mark all moving bricks as such
			if(v==ID) bricks[n]->drawback();		// draw indestructibles to background
			bricks[n]->draw();			// draw any bricks
		}
		setup_sides();
		text tlevel("Level:", SHADOW);
		tlevel.position(264, 144);
		sprintf(leveltext, "%02d", level);
		text nlevel(leveltext, SHADOW);
		nlevel.position(282, 156);

		text tscore("Score:", SHADOW);
		tscore.position(264, 170);
		sprintf(scoretext, "%07ld", score);
		text nscore(scoretext, SHADOW);
		nscore.position(260, 182);


		tscore.draw();
		nscore.draw();
		tlevel.draw();
		nlevel.draw();

		lifegraph->draw();
		draw_lives(lifegraph, lives);
		revgraph->draw();
		draw_revbar(revgraph, 0);
		// if bonus bricks are drawn from prev level, erase them
		for(i=0; i<4; i++) {
			if((bonusbits&(1<<i))==(1<<i)) bonus[i].erase();
		}
		bonusbits=0;

		Fade2Palette(MainGame->Palette);	// Fade in colour scheme
		finished=0;
		balls[0]->undelete();
		balls[0]->position((bat.getx()+(bat.getwidth()/2))-(BALLWIDTH/2),bat.gety()-BALLHEIGHT);
		bat.draw();
		balls[0]->draw();
		balls[0]->setspeed(level_speed[level]); // start at current level speed
		balls[0]->setdead(0);
		for(i=1;i<NumBricks;i++) balls[i]->setdead(1); // mark all balls above 1 dead
		pball=250;	// delaly for positioning ball
		batreverse=0;	// bat movement not reversed
		numballs=1;

		while(!finished) {
			WaitRetrace();
			for(b=0;b<oldnumballs;b++) {
				if(balls[b]->dead()) continue;
				balls[b]->erasemask();
				balls[b]->erase(); // erase all moving balls
				balls[b]->draw();    	// *
				balls[b]->drawmask();	// *
				if(globalspeed==0) continue;
				else if(globalspeed<0) balls[b]->decspeed();
				else balls[b]->incspeed();
			}
			for(i=0;i<balldelarray.entries();i++) {
				n=balldelarray.value(i);
				balldelarray.remove(i);
				balls[n]->setdead(1);	// mark ball dead
				balls[n]->erase();    	// *
				balls[n]->erasemask();	// *
				i--;
			}
			if(laserflag && laserfired) {	// if laser on and fired
				laser.erase();
				laser.draw();
			}
			oldnumballs=numballs;
			bat.erase(); bat.draw();
			if(laserflag && !laserfired) {	// if laser enabled but not fired
				lindicator.position(bat.getx()+(bat.getwidth()/2)-1, bat.gety()+1);
				lindicator.draw();
			}
			for(i=0;i<movearray->entries(); i++) {
				n=movearray->value(i);
				t=bricks[n]->getdir();
				int ix=bricks[n]->getoldx(); int iy=bricks[n]->getoldy();
				//** next line was SCRBOT-DEFHEIGHT
				if(t || bricks[n]->y>(SCRBOT-1)) bricks[n]->erasemask();	// if sideways mover, rem mask
				bricks[n]->erase();
				if(!t) {
					if(bricks[n]->gethit()) {
						bricks[n]->droperasemask();	// if just hit, remove from mask
						bricks[n]->clearhit();
					}
					v=getMaskPixel(ix+8, iy-1); // top pixel
					if(v) bricks[v-1]->draw();	// redraw top overlaid brick
					v=getMaskPixel(ix+8,iy+DEFHEIGHT-1);
					if(v) bricks[v-1]->draw();	// redraw bottom overlaid brick
				}
				if(bricks[n]->deleted()) {	// if marked for deletion
					if(bricks[n]->gettype()==SS) numneeded--;
					movearray->remove(i);
					i--;
					continue;		// then remove and continue;
				}
				bricks[n]->draw();
				if(t) bricks[n]->drawmask();	// if sidewaysmover, draw mask
			}

			// Now to move animation on for exploding bricks
			for(i=0;i<explodearray.entries(); i++) {
				n=explodearray.value(i);	// get brick number
				if(bricks[n]->explodenum==FAKEANI) { // start of explosion seq
					bricks[n]->erasemask();	// remove from mask so no more collisions
					bricks[n]->explodenum=3;	// start on frame 0
					bricks[n]->resize(DEFWIDTH, DEFHEIGHT, exploding_graphics[0]);
					bricks[n]->draw();	// draw new graphics
					bricks[n]->setdelay();	// set count down delay
					continue;
				} else {	// in middle of animation
					if(bricks[n]->getdelay()==0) {	// if at end of delay
						bricks[n]->explodenum++;	// onto next animation frame
						t=bricks[n]->explodenum;
						if(t==MAXFRAME) {	// if on big explode value
							bricks[n]->resize(DEFWIDTH, DEFHEIGHT, \
							  exploding_graphics[t]);	// setup graphics for brick
							bricks[n]->draw();	// redraw new graphics
							bricks[n]->setdelay();	// set max delay again
							// Now draw the surrounding explosion
							tx=bricks[n]->getx();
							ty=bricks[n]->gety();
							b=bricks[n]->getside(0);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoDrawAbove;
							}
							blitSprite(tx,ty-DEFHEIGHT, DEFHEIGHT, DEFWIDTH, \
							 exploding_graphics[t]); // draw above

NoDrawAbove:                		b=bricks[n]->getside(1);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoDrawRight;
							}
							blitSprite(tx+DEFWIDTH,ty, DEFHEIGHT, DEFWIDTH, \
							 exploding_graphics[t]); // draw right

NoDrawRight:				b=bricks[n]->getside(2);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoDrawBelow;
							}
							blitSprite(tx,ty+DEFHEIGHT, DEFHEIGHT, DEFWIDTH, \
							 exploding_graphics[t]); // draw below

NoDrawBelow:				b=bricks[n]->getside(3);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoDrawLeft;
							}
							blitSprite(tx-DEFWIDTH,ty, DEFHEIGHT, DEFWIDTH, \
							 exploding_graphics[t]); // draw left
NoDrawLeft:
						} else if(t>MAXFRAME) { // undraw time and collision check
							delarray.add(n);	// mark block deleted
							bricks[n]->dele();	// mark block itself deleted
							explodearray.remove(i);	// remove from array
							i--;	// and update counter to show
							// Now undraw around sides and redraw if ID
							tx=bricks[n]->getx();
							ty=bricks[n]->gety();
							b=bricks[n]->getside(0);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoEraseAbove;
							}
							eraseSprite(tx,ty-DEFHEIGHT, DEFHEIGHT, DEFWIDTH, \
							 backptr); // erase above

NoEraseAbove:				b=bricks[n]->getside(1);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoEraseRight;
							}
							eraseSprite(tx+DEFWIDTH,ty, DEFHEIGHT, DEFWIDTH, \
							 backptr); // erase right

NoEraseRight:				b=bricks[n]->getside(2);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoEraseBelow;
							}
							eraseSprite(tx,ty+DEFHEIGHT, DEFHEIGHT, DEFWIDTH, \
							 backptr); // erase below

NoEraseBelow:				b=bricks[n]->getside(3);
							if(b!=-1) {
							 if(bricks[b]->gettype()==ID) goto NoEraseLeft;
							}
							eraseSprite(tx-DEFWIDTH,ty, DEFHEIGHT, DEFWIDTH, \
							 backptr); // erase left

NoEraseLeft:
							for(v=0;v<NUMSIDES;v++) {	// now check surrounding bricks
								b=bricks[n]->getside(v); // b is used to store side no
								if(b==-1) continue;	// skip if no brick there
								// Now check not deleted or moving
								if(!bricks[b]->deleted() && !bricks[b]->moving()) {
									int tey=bricks[b]->gettype();	// get type of brick
									if(tey==ID) {	// indestructible brick
										bricks[b]->draw();	// redraw indestructible bricks
									} else if(tey==AB) {	// Another ball bricks
										add_newball(bricks[b]->getx()+(DEFWIDTH/2),  \
													bricks[b]->gety()+(DEFHEIGHT/2), \
													numballs, level);
									} else if(tey==MB) { // Murder ball released
										add_newball(bricks[b]->getx()+(DEFWIDTH/2),  \
													bricks[b]->gety()+(DEFHEIGHT/2), \
													numballs, level);
										balls[numballs-1]->settype(MURDERBALL, murderball_sprite);
									} else if(tey==OF) { // lights off, activate
										if(lightflag) { //if not already off
											Fade2Dark(Palette, MainGame->Palette);
											lightflag=0;
										}
									} else if(tey==ON) { // lights on
										if(!lightflag) { // if not already on
											FadeFromDark(Palette, MainGame->Palette);
											lightflag=1;
										}
									} else if(tey==BO) { // exploding brick type
										explodearray.add(b);	// mark exploding
									}
									if(bricks[b]->process_explosion()) {	// process explosion hit
										movearray->add(b);
									}
									if(bricks[b]->deleted()) delarray.add(b); // mark deleted if so
								} // end if
							} // end for
						} else {	// more frames left, redraw
							bricks[n]->resize(DEFWIDTH, DEFHEIGHT, \
							  exploding_graphics[t]);	// setup graphics for brick
							bricks[n]->draw();	// redraw new graphics
							bricks[n]->setdelay();	// set max delay again
							continue;
						}
					} else {	// not yet counted down delay
						bricks[n]->decdelay();	// onto next delay value;
					}
				}
			}

			for(t=0;t<delarray.entries();t++) { // if block to delete in array
				n=delarray.value(t);	// get first entry
				if(bricks[n]->moving()) {
					delarray.remove(t);
					t--;
					continue;
				}
				bricks[n]->erasemask();
				bricks[n]->erase();  // not moving so delete
				delarray.remove(t);		// then remove it
				i=bricks[n]->gettype();
				if(i>NB && i<=RB) numneeded--;
				t--;
			}
			globalspeed=0;
			batcollide=0;
			// Bat movement routines
			for(b=0;b<oldnumballs;b++) {	// loop for number of balls
				if(pball) break;	// skip all loop if positioning batball
				if(balls[b]->deleted()) continue; // skip deleted balls
				speed=balls[b]->getspeed();
				while(speed) {
					loop=speed>10?10:speed;
					bx=balls[b]->getx()+1; by=balls[b]->gety()+1; // ball centre
					af=bf=cf=0;
					dir=balls[b]->getdir()/90;
					olddir=balls[b]->getdir();
					switch(dir) {	// switch based on current direction
						case 0:	// upper right  quadrant
							bx++; by--;	// reposition to UR corner pixel
							h1=getMaskPixel(bx-1, by);
							h2=getMaskPixel(bx, by);
							h3=getMaskPixel(bx, by+1);
							d1=BOT;
							d2=LEF;
							break;
						case 1:
							bx++; by++;	// reposition to LR corner pixel
							h1=getMaskPixel(bx, by-1);
							h2=getMaskPixel(bx, by);
							h3=getMaskPixel(bx-1, by);
							d1=LEF;
							d2=TOP;
							break;
						case 2:
							bx--; by++;	// reposition to LL corner pixel
							h1=getMaskPixel(bx+1, by);
							h2=getMaskPixel(bx, by);
							h3=getMaskPixel(bx, by-1);
							d1=TOP;
							d2=RIG;
							break;
						case 3:
							bx--; by--;	// reposition to UL corner pixel
							h1=getMaskPixel(bx, by+1);
							h2=getMaskPixel(bx, by);
							h3=getMaskPixel(bx+1, by);
							d1=RIG;
							d2=BOT;
							break;
						default:	// should be never reached
							break;
					}
					if(h1) {
						if(!bricks[h1-1]->deleted()) {
							if(bricks[h1-1]->issolid()) af=h1;
							else af=0;
						} else h1=0;
					}
					if(h2) {
						if(!bricks[h2-1]->deleted()) {
							if(bricks[h2-1]->issolid()) bf=h2;
							else bf=0;
						} else h2=0;
					}
					if(h3) {
						if(!bricks[h3-1]->deleted()) {
							if(bricks[h3-1]->issolid()) cf=h3;
							else cf=0;
						} else h3=0;
					}
					if(af && cf) balls[b]->simplecol(DIA);
					else if(bf && (!(af || cf))) balls[b]->simplecol(DIA);
					else if(af) balls[b]->simplecol(d1);
					else if(cf) balls[b]->simplecol(d2);
					if(h2) {	// if a diagonal hit
						if(h1==h2) h1=0;	// check for duplicate values
						if(h3==h2) h3=0;	// and cancel them out
					}
					if(af && bf && cf) {	// now check for solid bricks
						v=bricks[af-1]->gettype();	// surrounding normal brick
						t=bricks[cf-1]->gettype();	// i.e. no entry
						if((v==ID || v==T5) && (t==ID || t==T5)) h2=0;
					}
					t=0;
					if(h1) {
						hitarray[t]=h1-1;
						t++;
					}
					if(h2) {
						hitarray[t]=h2-1;
						t++;
					}
					if(h3) {
						hitarray[t]=h3-1;
						t++;
					}
					for(i=0;i<t;i++) {
						n=hitarray[i];
						// skip if already moving down
						if(bricks[n]->moving()) bricks[n]->sethit();	// set hit flag on moving bricks
						if(bricks[n]->moving() && !bricks[n]->getdir()) continue;
						v=bricks[n]->gettype();
						if(v==AB) {
							add_ball(b, numballs);	// add new ball
							balls[numballs-1]->setdir(olddir);
						} else if(v==MB) {
							add_ball(b, numballs);	// add new ball
							balls[numballs-1]->setdir(olddir);
							balls[numballs-1]->settype(MURDERBALL, murderball_sprite);
						}
						else if(v==IF) balls[b]->incspeed();
						else if(v==IS) balls[b]->decspeed();
						else if(v==BO) { // bomb brick
							if(!bricks[n]->is_exploding()) {
								explodearray.add(n);	// add hit brick to array
							}
							// **** Warning, check not already exploding. DONE
						// Now add on the score
						} else if(v<=RB || v==SS) {
							if(v==RB) score+=100L;
							else if(v==SS) score+=500L;
							else score+=(unsigned long)v*10L;
							sprintf(scoretext, "%07ld", score);
							nscore.erase();
							nscore.setstring(scoretext);
							nscore.draw();
						}
						if(bricks[n]->process_hit()) {	// Mark block as moving down screen
							movearray->add(n);
						}
						if(bricks[n]->deleted()) delarray.add(n);
					}
					// First check if hit left wall
					if((balls[b]->getx() < LeftBorderEndX) && (balls[b]->xdir==-1)) balls[b]->simplecol(RIG); //
					// then check for right wall
					if((balls[b]->getx()+BALLWIDTH > RightBorderX) && (balls[b]->xdir>0)) balls[b]->simplecol(LEF);
					// lastly check top wall
					if((balls[b]->gety() < TopBorderEndY) && (balls[b]->ydir<0)) balls[b]->simplecol(BOT);

					if(bat.collision_bat(*balls[b])) {
						if(balls[b]->gettype()==MURDERBALL) {
							finished=2;
							done=4;
							break;
						} else balls[b]->batcol(mousemove);
					}
					balls[b]->updatepos(loop);
					if(speed>10) speed-=10; else speed=0;
					if(balls[b]->y>197) break;	// exit inner loop if past bottom
				}

			}
			b=0;
			if(laserflag && laserfired) {	// if a laser on screen
				bx=laser.getx()+1; by=laser.gety(); // top centre
				h1=getMaskPixel(bx-1, by);	// top left
				h2=getMaskPixel(bx, by);	// top centre
				h3=getMaskPixel(bx+1, by);	// top right
				if(h2) {
					if(h1==h2) h1=0;	// check for duplicate values
					if(h3==h2) h3=0;	// and cancel them out
				}
				t=0;
				if(h1) {
					if(!bricks[h1-1]->deleted()) { 	// if not deleted
						hitarray[t]=h1-1;
						t++;
					}
				}
				if(h2) {
					if(!bricks[h2-1]->deleted()) { 	// if not deleted
						hitarray[t]=h2-1;
						t++;
					}
				}
				if(h3) {
					if(!bricks[h3-1]->deleted()) { 	// if not deleted
						hitarray[t]=h3-1;
						t++;
					}
				}
				for(i=0;i<t;i++) {
					b=1;
					n=hitarray[i];
					// skip if already moving down
					if(bricks[n]->moving()) bricks[n]->sethit();	// set hit flag on moving bricks
					if(bricks[n]->moving() && !bricks[n]->getdir()) continue;
					v=bricks[n]->gettype();
					if(v==AB) {
						add_newball(laser.getx(), laser.gety(), numballs, level);	// add new ball
					} else if(v==MB) {
						add_newball(laser.getx(), laser.gety(), numballs, level);	// add new ball
						balls[numballs-1]->settype(MURDERBALL, murderball_sprite);
					}
					else if(v==IF) globalspeed=1;
					else if(v==IS) globalspeed=-1;
					else if(v==BO) { // bomb brick
						if(!bricks[n]->is_exploding()) {
							explodearray.add(n);	// add hit brick to array
						}
						// **** Warning, check not already exploding. DONE
					// Now add on the score
					} else if(v<=RB || v==SS) {
						if(v==RB) score+=100L;
						else if(v==SS) score+=500L;
						else score+=(unsigned long)v*10L;
						sprintf(scoretext, "%07ld", score);
						nscore.erase();
						nscore.setstring(scoretext);
						nscore.draw();
					}
					if(bricks[n]->process_hit()) {	// Mark block as moving down screen
						movearray->add(n);
					}
					if(bricks[n]->deleted()) delarray.add(n);
				}
				if(b) {
					laser.erase();
					laserfired=0;
				}
			}
			nummoving=movearray->entries();
			for(i=0; i<nummoving; i++) {
				n=movearray->value(i);	// get brick number from array
				if(bricks[n]->deleted()) continue;
				if(bricks[n]->moving()) {	// If brick is already hit
				  if(bricks[n]->move()) {		// move down a line
					if(bricks[n]->gety()>=SCRBOT-1) {
						bricks[n]->dele();
						continue;
					}
					batcollide=bricks[n]->collision(bat);	// hit the bat?
					if(batcollide!=0) {
						switch(bricks[n]->gettype()) {
							case LB:	// extra life brick??
								DoBeep(800, 50);
								if(lives<99) lives++;	// increase lives
								draw_lives(lifegraph, lives);
								bricks[n]->dele();
								batcollide=0;
								break;
							case SB:	// slow down brick
								DoBeep(800,50);
								globalspeed=-1;
								bricks[n]->dele();
								batcollide=0;
								break;
							case FB:	// speed up brick
								DoBeep(800,50);
								globalspeed=1;
								bricks[n]->dele();
								batcollide=0;
								break;
							case B1:	// bonus 1 brick
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								if(!(bonusbits&0x01)) { //not drawn yet
									bonus[0].draw();
									bonusbits|=0x01;	// set selected bit
								}
								batcollide=0;
								break;
							case B2:	// bonus 2 brick
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								if(!(bonusbits&0x02)) { //not drawn yet
									bonus[1].draw();
									bonusbits|=0x02;	// set selected bit
								}
								batcollide=0;
								break;
							case B3:	// bonus 3 brick
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								if(!(bonusbits&0x04)) { //not drawn yet
									bonus[2].draw();
									bonusbits|=0x04;	// set selected bit
								}
								batcollide=0;
								break;
							case B4:	// bonus 4 brick
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								if(!(bonusbits&0x08)) { //not drawn yet
									bonus[3].draw();
									bonusbits|=0x08;	// set selected bit
								}
								batcollide=0;
								break;
							case B5:	// bonus 5 brick
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								if(!(bonusbits&0x10)) { //not drawn yet
									bonus[4].draw();
									bonusbits|=0x10;	// set selected bit
								}
								batcollide=0;
								break;
							case BA:	// bonus 1000 brick
								DoBeep(800,15);
								DoBeep(1000,15);
								DoBeep(1200,15);
								bricks[n]->dele();
								score+=1000L;
								sprintf(scoretext, "%07ld", score);
								nscore.erase();
								nscore.setstring(scoretext);
								nscore.draw();
								batcollide=0;
								break;
							case OF:	// lights off brick
								if(!lightflag) break;	// continue if already off
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								Fade2Dark(Palette, MainGame->Palette);
								lightflag=0;	// now lights off
								batcollide=0;
								break;
							case ON:	// lights on brick
								if(lightflag) break;	// continue if already on
								DoBeep(1000,25);
								DoBeep(1000,25);
								bricks[n]->dele();
								FadeFromDark(Palette, MainGame->Palette);
								lightflag=1;	// now lights off
								batcollide=0;
								break;
							case LA:	// give laser fire brick
								if(laserflag) break;	// if already got it
								DoBeep(1000,25);
								DoBeep(1000,25);
								laserflag=1;
								laserfired=0;
								bricks[n]->dele();
								batcollide=0;
								break;
							case RE:	// bat reverse brick
								DoBeep(800, 30);
								DoBeep(800, 30);
								batreverse=!batreverse;	// switch flag
								batcountdown=BATCOUNT;	// set count down timer
								oldrevwidth=BATCOUNT*2;
								bricks[n]->dele();
								batcollide=0;
								break;
							case RW:	// level rewind brick
								if(level>1) {
									done=finished=5;	// mark level rewind command
									DoBeep(250,100);
									DoBeep(200,100);
								} else {
									DoBeep(100, 50);
								}
								bricks[n]->dele();
								batcollide=0;
								break;
							case FO:	// level forward brick
								if(level<toplevel) {
									done=5;finished=6;	// mark level forward command
									DoBeep(250,100);
									DoBeep(200,100);
								} else {
									DoBeep(100, 50);
								}
								bricks[n]->dele();
								batcollide=0;
								break;
							case BS:	// make bat smaller
								if(batsize==0) {	// if already min size
									DoBeep(100,50);
									bricks[n]->dele();	// erase falling brick
									batcollide=0;
									break;
								}
								batsize--;
								DoBeep(800,50);
								bricks[n]->dele();	// erase falling brick
								bat.erase();	// erase the bat
								bat.position(bat.getx()+5, bat.gety());
								if(batsize==0) {	// small bat, reshape to that
									bat.resize(BATWIDTH-10, BATHEIGHT, bat_small);
									bat.draw();
								} else {			// else make normal size
									bat.resize(BATWIDTH, BATHEIGHT, bat_normal);
									bat.draw();
								}
								batcollide=0;
								break;
							case BB:	// make bat bigger
								if(batsize==2) {	// if already max size
									DoBeep(100,50);
									bricks[n]->dele();	// erase falling brick
									batcollide=0;
									break;
								}
								batsize++;
								DoBeep(800,50);
								bricks[n]->dele();	// erase falling brick
								bat.erase();
								bat.position(bat.getx()-5, bat.gety());
								if(batsize==2) {	// small bat, reshape to that
									bat.resize(BATWIDTH+10, BATHEIGHT, bat_big);
									bat.draw();
								} else {			// else make normal size
									bat.resize(BATWIDTH, BATHEIGHT, bat_normal);
									bat.draw();
								}
								batcollide=0;
								break;
							default:
								finished=2;		// Loose a life time
								done=4;
								break;
						}	// end switch
					} // end if batcollide
				  } else { // end of if(move())
					  // Now to check if moving horizontally and move if possible
					  tx=bricks[n]->getx();
					  ty=bricks[n]->gety();	// get ball X and y value
					  t=bricks[n]->getdir();	// get direction of ball
					  if(tx<=LeftBorderEndX && t<0) { // at left moving left
						bricks[n]->setdir(1);	  // set moving right
						continue;	// and continue loop
					  } else if(tx+DEFWIDTH>=RightBorderX && t>0) { // at right moving right
						bricks[n]->setdir(-1);	// set moving left
						continue;
					  }
					  if(t==1) {	// moving right
						// check if object to right and move left if there
						v=getMaskPixel(tx+DEFWIDTH, ty);
						if (v) bricks[n]->setdir(-1); // any other value
						else {	// if space to move into, check no ball and move if space
							if(testMaskLine(tx+DEFWIDTH, ty)) {
								if(getMaskPixel(tx+DEFWIDTH+BALLWIDTH, ty+4)) bricks[n]->setdir(-1);
								else bricks[n]->sethit();
							}
							else bricks[n]->moveside();	// else move right
						}
					  } else if(t==-1) {
						v=getMaskPixel(tx-1, ty);
						if (v) bricks[n]->setdir(1);
						else { // if space to move into, make sure no ball in the way
							if(testMaskLine(tx-1, ty)) {
								if(getMaskPixel(tx-BALLWIDTH, ty+4)) bricks[n]->setdir(1);
								else bricks[n]->sethit();
							}
							else bricks[n]->moveside();	// else move sideways
						}
					  }
				  }
				} // end if moving
			} // end moving loop

			if(numneeded<=0) {
				if(testmode) finished=done=6; // exit game at level end
				else finished=done=1;    // done, all bricks dead
			}
			key=position_bat(bat);

			mousearray.remove(0);	// remove first element in array
			mousearray.add(key); // and add current value to array
			mousemove=mousearray.highest(MLIMIT);	// get highest move average
			if(pball) {	// if about to position bat_ball
				balls[0]->sitonbat(&bat);
				pball--;
			}
			// check for escape to exit program
			if(bioskey(1)) {
				if(bioskey(0)==ESCAPE) finished=done=3;
			}
			// check fir left button to release a carried ball
			t=MouseButton() & BUTTON1;
			if ((t == BUTTON1) && pball) { // release to left
				pball=0;
				balls[0]->setdir(325);
			}
			else if (t == BUTTON1) {	// if pball not on, check for laser
				if(laserflag && !laserfired) {	// if laser on but not fired
					laser.position(bat.getx()+(bat.getwidth()/2),bat.gety()-2);
					laser.draw();
					laserfired=1;
				}
			}
			t=MouseButton() & BUTTON2;
			if ((t == BUTTON2) && pball) { // release to right
				pball=0;
				balls[0]->setdir(35);
			}

			if(laserflag && laserfired) {	// check for top border
				laser.y=laser.y-2;	// move up a line
				// and check to see if hit top of screen
				if(laser.gety()<=TopBorderEndY) {
					laser.erasemask();
					laser.erase();
					laserfired=0;
				}
			}

			t=0;
			for(b=0;b<numballs;b++) {
				if(balls[b]->dead()) continue;
				if(balls[b]->gety()>=197) { // of bottom of screen
					balldelarray.add(b);	// delete ball
					balls[b]->dele();		// mark deleted
				} else if(balls[b]->gettype()==NORMBALL) t=1;
			}
			if(t==0) finished=done=2;
			if(batreverse) { // if reverse effects in operation
				batcountdown--;	// decrement counter
				if(batcountdown==0) {
					batreverse=!batreverse;	// cancel flag
					DoBeep(600, 30);
					delay(30);
					DoBeep(600, 30);
					delay(30);
				} else {
					draw_revbar(revgraph, batcountdown);
				}
			}

			// check for bonus just completed
			if((bonusbits&0x1f)==0x1f) {	// all bricks set
				DoBeep(600,50);
				delay(50);
				DoBeep(800,50);
				delay(50);
				DoBeep(1000,50);
				delay(50);
				for(unsigned i=50; i<level*250;i+=50) {
					DoBeep(i, 15);
					delay(10);
					sprintf(scoretext, "%07ld", score+(unsigned long)i);
					nscore.erase();
					nscore.setstring(scoretext);
					nscore.draw();
				}
				score+=250L*(unsigned long)level;	// Bonus add onto score
				bonusbits=0;
				for(i=0; i<5; i++) {	// now undraw bonus bricks
					bonus[i].erase();
				}
			}

			if(done==4) { // block has hit the bat?
				DoBeep(50,100);
				delay(100);
				DoBeep(50,100);
				delay(100);
				bat.erasemask();
				for(i=bat.gety(); i<SCRBOT-1; i++) {
					DoBeep(i*50, 25);
					bat.y=bat.y+1;
					bat.erase();
					bat.draw();
					delay(100);
				}
				bat.erase();
				bat.position((RIGHTBORDER-bat.getwidth())/2,185);

			}
			if(finished==2) { // ball at screen bottom?
				lives--;
				draw_lives(lifegraph, lives);
				DoBeep(200,200);
				delay(200);
				if(lives==0) continue;
				finished=0;
				for(b=0;b<numballs;b++) {
					if(balls[b]->dead()) continue;
					balls[b]->erasemask();
					balls[b]->erase();
				}
				t=movearray->entries();
				for(n=0;n<t;n++) {	// Clear all falling bricks
					i=movearray->value(n);	// get all moving brick entries
					v=bricks[i]->gettype();
					if(v==SS || v==SF) continue;  // don'd delete horizontal move bricks
					bricks[i]->dele();	// mark block to be deleted
				}
				balls[0]->undelete(); balls[0]->setdead(0);	// undelete
				balls[0]->setspeed(level_speed[level]);
				// and make sure not in ball delarray
				for(i=0;i<balldelarray.entries();i++) {
					if(balldelarray.value(i)==0) {
						balldelarray.remove(i);
						break;
					}
				}
				oldnumballs=numballs=1;
				pball=250;
				if(laserflag && laserfired) {
					laser.erase();
				}
				laserflag=0;	// turn laser off
				laserfired=0;	// and not been fired
				done=0;
			}

		}

		if(done==1) {	// Level Completed??
			DoBeep(300,100);
			delay(100);
			DoBeep(600,100);
			delay(100);
			DoBeep(900,100);
			delay(100);
			for(int i=20; i<1000;i+=20) {
				DoBeep(i, 15);
				delay(10);
				sprintf(scoretext, "%07ld", score+(unsigned long)i);
				nscore.erase();
				nscore.setstring(scoretext);
				nscore.draw();
			}
			score+=1000L;	// End of level bonus

			done=finished=0;		// Reset done counter
			scorelevel*=2;		// Double score per brick
			level++;
			delarray.clear();
			movearray->clear();
			explodearray.clear();
			firsttime=1;
		}
		if(done==5) {	// level wind command (forward or back)
			DoBeep(900,100);
			delay(100);
			DoBeep(600,100);
			delay(100);
			DoBeep(300,100);
			delay(100);
			done=finished=0;		// Reset done counter
			if(finished==5) {	// if moving back a level
				scorelevel/=2;		// Halve score per brick
				level--;
			} else  {
				scorelevel*=2;
				level++;
			}
			delarray.clear();
			movearray->clear();
			explodearray.clear();
			firsttime=1;
		}
		NoBeep();	// turn off pending sounds

	}
	deallocateballs();
	deallocatelevels();
	deallocatebricks();
	ProgramTimer0(0);	// back to 18.2 ticks a second
	Fade2Black(MainGame->Palette);
	if(done!=3 && !testmode) { // if not ESCAPE or testmode exit
		ClearScreen();
		DrawText("Game Over!", 120, 96, 2); // say game over
		Fade2Palette(MainGame->Palette);
		delay(2000); // wait 2 seconds
		Fade2Black(MainGame->Palette);
		lastscore=score;
		lastlevel=level;
		exitnormal=TRUE;	// setup check for high score
	} else {
		exitnormal=FALSE;
	}
	delete MainGame->ImageData;	// free image area
	delete MainGame;
	delete backptr;
	delete ballbuffer;
	delete maskbuffer;
	RestoreTimer();
	// finally free memory allocated to level array
	delete movearray;	// deallocate spce for move array
	delete lifegraph;
	delete revgraph;	// delete additional graphic objects
	delete Palette;
	return 0;
}

// Routine to allow on screen editing of a text box
void editstring(text *instring)
{
	// first allocate memory for copy of string and editing string
	char far *origstring = new char [strlen(instring->getstring())+1];
	char far *editstring = new char [MAXNAMESIZE+2];
	strcpy(origstring, instring->getstring()); // now copy over to buffers
	strcpy(editstring, origstring);		   // and for editing
	strcat(editstring, "_");	// add edit symbol (_) to string
	instring->erase();	// erase old string
	instring->setcolour(NAMEEDITCOLOUR);	// change colour to signify editing
	instring->setstring(editstring);	// change to editing string
	instring->draw();	// and redraw
	int noexit=TRUE;	// exit loop flag
	int c;			// key received character
	int slen;
	while(noexit) {
		if(bioskey(1)) {	// if a key is pressed
			c=bioskey(0)&0xff;	// read character into c
			if(c=='\b') {	// backspace pressed
				slen=strlen(editstring);
				if(slen>1) { // if something to delete
					// then shrink string down
					editstring[slen-2]='_';
					editstring[slen-1]='\0'; // terminate string
				}
			} else if(c=='\x1b') {	// escape pressed
				noexit=FALSE;
				break;
			} else if(c=='\r') {	// return pressed
				noexit=FALSE;
				break;
			} else if(c<32 || c>127) { //ignore outside char range
				continue;
			} else {	// valid character pressed
				slen=strlen(editstring);
				if(slen-1<MAXNAMESIZE) { // space to add character??
					editstring[slen-1]=char(c); // add character
					editstring[slen]='_'; // add prompt char
					editstring[slen+1]='\0'; // terminate string
				}
			}
			instring->erase();		// erase string
			instring->setstring(editstring); // change to new info
			instring->draw();		// and redraw it
		} // end of bioskey loop
	} // end of while loop
	// If got here then escape or return pressed, check which
	instring->erase();	// erase old string
	instring->setcolour(NAMECOLOUR);	// set back to normal colour
	if(c=='\r') {	// if exit via enter
		slen=strlen(editstring);
		editstring[slen-1]='\0';	// strip prompt character
	} else {	// else escape pressed, return to original value
		strcpy(editstring, origstring);	// copy back original string
	}
	instring->setstring(editstring);	// set to new value
	instring->draw();			// redraw the string
	delete editstring;	// deallocate memory for edit buffer
	delete origstring;	// and also backup buffer
	return;
}

void allocatelevels(void)
{
	for(int i=0; i<=MAXLEVEL; i++) {
		level_data[i]=new char[NumBricks+1];
		level_name[i]=new char[MAXNAMESIZE+1];
	}
}

void deallocatelevels(void)
{
	for(int i=0; i<=MAXLEVEL; i++) {
		delete level_data[i];
		delete level_name[i];
	}
}

// Routine to clear level data, name and speed arrays.  ** be careful **
void cleararrays(void)
{
	int i;

	for(i=0;i<MAXLEVEL;i++) {	// loop to clear level arrays
		level_speed[i]=DEFAULTSPEED;
	}
}

// routine to allocate memory for each brick
void allocatebricks(void)
{
	for(int i=0;i<NumBricks;i++) {
		bricks[i]=new block;	// allocate memory for brick
		if(bricks[i]==NULL) {
			errhandler("Out of memory allocating brick storage.", 8);
		}
	}
}

// routine to allocate memory for each ball
void allocateballs(void)
{
	for(int i=0;i<NumBricks;i++) {
		balls[i]=new ballblock;	// allocate memory for brick
		if(balls[i]==NULL) {
			errhandler("Out of memory allocating ball storage.", 9);
		}
	}
}

// routine to deallocate memory for bricks
void deallocatebricks(void)
{
	for(int i=0;i<NumBricks;i++) {
		if(bricks[i]!=NULL) delete bricks[i];
	}
}

// routine to deallocate memory for balls
void deallocateballs(void)
{
	for(int i=0;i<NumBricks;i++) {
		if(balls[i]!=NULL) delete balls[i];
	}
}

// Routine to fill in current level from array, return 0 on error or level no
int create_level(int level)
{
	int i;
	int val;

	if(level_data[level]==NULL) return 0;	// no level at that address
	i=level;
	for(i=0; i<NumBricks; i++) {
		val=*(level_data[level]+i);	// get current entry value
		bricks[i]->resize(DEFWIDTH, DEFHEIGHT, brick_pointers[val]);
		bricks[i]->settype(val, brick_pointers[val]);
		bricks[i]->setid(i+1);		// set brick mask id
	}
	return level;	// return level number read
}

// Routine to fill in surrounding bricks of level (around array of brick)
void setup_sides(void)
{
	int i;

	for(i=0;i<NumBricks;i++) {
		bricks[i]->setside(0, above(i));		// fill in brick above value
		bricks[i]->setside(1, toright(i));	// and to right
		bricks[i]->setside(2, below(i));		// and below
		bricks[i]->setside(3, toleft(i));	// and finally to left
	}
}

// Routine to move bricks objects into the array entry
void blocks_to_array(int level)
{
	int i;
	int val;

	if(level_data[level]==NULL) return;	// level not created
	for(i=0; i<NumBricks; i++) {
		val=bricks[i]->gettype();
		*(level_data[level]+i)=val;			// fill array entry
	}
	return;
}

// read level routine, returns level no or 0 on error
int read_levels(void)
{
	int i, l, t;
	int maxlevel=0;
	int n, level;
	int speed;

	char temp[80];
	char tempname[80];

	ifstream fin(levelfile);	// open level file for input
	if(fin.fail()) {	// file not opened sucessfully
		errhandler("Level data file LEVELS.DAT is missing or corrupt", 10);
	}
	fin.seekg(0);
	fin.clear();
	while(!fin.eof()) {
		fin >> temp;	// read first word of input
		if(strcmp(temp, "MAXLEVEL")==0) {	// if found
			fin >> maxlevel;	// read max level value
			break;
		}
	}
	if(maxlevel>50) return -1;		// maximum level to great
	fin.seekg(0);
	fin.clear();
	for(l=1;l<=maxlevel;l++) {	// loop to read all levels
		while(!fin.eof()) {
			fin >> temp; // look for LEVEL keyword
			if(strcmp(temp, "LEVEL")==0) {	// search for level indicator
				fin >> level;	// read in level number
				if(l==level) break;	// if found exit loop
			}
		}
		if(level!=l) {	// request level not found
			errhandler("Requested level missing, possible corrupted level file.", 11);
		}
		speed=0;
		fin >> temp; // look for SPEED keyword
		if(strcmp(temp, "SPEED")==0) {	// search for speed text
			fin >> speed;
			level_speed[l]=speed;	// store into table
		}
		if(speed==0) { // no speed specifier found
			errhandler("Speed info missing from level info file.", 30);
		}


		fin >> temp; // look for NAME keyword
		if(strcmp(temp, "NAME")==0) { // search for name text
			fin.ignore(1);		// strip space after NAME
			fin.getline(tempname, 79);
			if(strlen(tempname)>0) { // if name specified
				strcpy(level_name[l], tempname);
			}
		}

		for(i=0; i<NumBricks; i++) {
			fin >> temp;	// read next value
			n=0;
			while(brick_shorthand[n]) {	// while a value in the array
				if(strcmp(temp, brick_shorthand[n])==0) break; // break on find
				n++;
			}
			if(brick_shorthand[n]==NULL) {	// no match found
				errhandler("Corrupted data in level file.  Illegal type.", 12);
			}
			*(level_data[l]+i)=n;	// stire value for level
		}
	}
	fin.close();	// close input file
	return maxlevel;		// and return max level number
}

// write levels to disk routine (all levels stored in level_data array).
int write_levels(void)
{
	int i, t, l;
	int maxlevel;
	int n;
	int val;

	ofstream fout(levelfile);	// open level file for writing
	if(fout.fail()) {	// if error opening output file
		errhandler("Error opening output file for level data.", 13);
	}
	t=1;
	while(level_data[t] && t<51) t++;	// count number of entries in array
	maxlevel=t-1;
	fout << "** Do not manually edit this file! **\n" << endl;
	fout << "MAXLEVEL " << maxlevel << endl << endl;
	for(l=1; l<=maxlevel; l++) {		// level loop array
		fout << "LEVEL " << l << endl;	// output level header
		if(level_speed[l]==0) {		// if no speed
			fout << "SPEED 20" << endl;	// set to default
		} else {
			fout << "SPEED " << level_speed[l] << endl; // output level speed
		}
		if(strlen(level_name[l])==0) { 	// if no level name
			fout << "NAME No Name" << endl; // set to NoName
		} else {
			fout << "NAME " << level_name[l] << endl; // output name
		}
		t=0;
		for(i=0;i<BricksY; i++) {
			fout << "\t";				// send out tab at line start
			for(n=0; n<BricksX; n++) {
				val=*(level_data[l]+t);	// get value in array
				fout << brick_shorthand[val] << " ";	// output type
				t++;
			}
			fout << endl;		// output end of line character
		}
		fout << endl;	// blank line between levels
	}
	fout.close();		// close the output file stream
	return 0;
}

// routine to add a new ball to end of array which is a copy of ball
int add_ball(int fromball, int toball)
{
	balls[toball]->settype(NORMBALL, ball_sprite);	// set type
	// then set to same position as the old ball, and same speed
	balls[toball]->position(balls[fromball]->getx(), \
							balls[fromball]->gety());
	balls[toball]->setspeed(balls[fromball]->getspeed());
	balls[toball]->setdir(balls[fromball]->getdir());	// same direction (for now)
	balls[toball]->setid(BALLID);
	balls[toball]->undelete();
	balls[toball]->setdead(0);	// mark it as not dead
	numballs++;
	return 0;
}

// routine to add a new ball to end of array which is at x,y. for laser hit
int add_newball(int nx, int ny, int toball, int level)
{
	balls[toball]->settype(40, ball_sprite);	// set type
	// then set to same position as the old ball, and same speed
	balls[toball]->position(nx, ny);
	balls[toball]->setspeed(level_speed[level]);
	balls[toball]->setdir(135);	// same direction (for now)
	balls[toball]->setid(BALLID);
	balls[toball]->undelete();
	balls[toball]->setdead(0);	// mark it as not dead
	numballs++;
	return 0;
}

// counts the number of bricks required to complere level
int count_required(void)
{
	int i, count=0;
	int tp;

	for(i=0; i<NumBricks; i++) {
		tp=bricks[i]->gettype();
		if(tp>NB && tp<=RB) count++;		// normal bricks required
		if(tp==SS) count++;		// normal moving is also required
	}
	return count;
}

int position_bat(block &bat)
{
	int key;

	key=MouseMotionX();
	// now test for bat reverse effects and correct movement
	if(batreverse) {
		if(key<0) key=-key;	// positive to negative value
		else if(key>0) key=0-key; // negative to positive value
	}

	if(key<0) {
		if(bat.getx()+key>LEFTBORDER) { // If bat not moving off left
			bat.x+=key;
		} else {        // if off left
			bat.x=LEFTBORDER;
		}
	} else if(key>0) {
		if(bat.getx()+bat.getwidth()+key<RIGHTBORDER) {
			bat.x+=key;
		} else {
			bat.x=RIGHTBORDER-bat.getwidth();
		}
	}
	return key;
}

// routine to draw correct number of lives on screen
void draw_lives(block *l, int lives)
{
	l->erase();	// erase first block
	if(lives==0) return;	// if no lives, do not redraw
	if(lives>4) lives=4;
	// Resize bat to correct one for number of lives
	l->resize(lives<4?(4+((lives-1)*LIFEGAP)):18, LIFEHEIGHT, life_graphics[lives]);
	l->draw();	// redraw the bat graphics
}

// routine to draw correct amount of reverse effect left on bar
void draw_revbar(block *r, int aleft)
{
	int t;

	t=aleft/REVSTEP;
	if(t==oldrevwidth) return;	// same as last value so return
	r->erase();	// erase the block cover
	// resize the covering bar
	r->resize(REVWIDTH-t, REVHEIGHT, reverse_bar);
	// and move bar to relevant position
	r->position(REVX+t, REVY);
	r->draw();	// and redraw
	oldrevwidth=t;
}

// routine to write high scores to disk
int write_highscores(void)
{
	FILE *fp;
	int i;

	fp=fopen("scores.dat", "w");
	if(fp==NULL) return 1;	// return error
	for(i=0;i<NUMSCORES;i++) {
		fwrite(&high_score[i], sizeof(long), 1, fp);	// score
		fwrite(&high_level[i], sizeof(char), 1, fp);	// level
		fputs(high_name[i], fp);			// name
		fputc('\n', fp);	       				// \newline
	}
	fclose(fp);
	return 0;	// return ok
}

// routine to read high scores from disk
int read_highscores(void)
{
	FILE *fp;
	int i;
	char tempstring[80];

	fp=fopen("scores.dat", "r");
	if(fp==NULL) {	// no highscore file so create one
		for(i=0; i<NUMSCORES;i++) {
			high_score[i]=(NUMSCORES-i)*1000L;
			high_level[i]=1;
			strcpy(high_name[i],"Steddy Man");
		}
		return 0; // return ok
	}
	for(i=0;i<NUMSCORES;i++) {
		fread(&high_score[i], sizeof(long), 1, fp);	// score
		fread(&high_level[i], sizeof(char), 1, fp);	// level
		fgets(tempstring, 79, fp);			// name
		*(strchr(tempstring, '\n'))='\0';	// strip \n
		strcpy(high_name[i], tempstring);
	}
	fclose(fp);
	return 0;	// return ok
}

// routine to check if a score is on highscore table
// will return entry number or -1 for none, also insert entry ready for editing
int check_highscore(unsigned long newscore, int endlevel)
{
	int i, n;

	for(i=0; i<NUMSCORES; i++) {
		if(newscore>high_score[i]) break; // exit if higher
	}
	if(i<NUMSCORES) {	// if on highscore table
		for(n=NUMSCORES-2; n>=i; n--) { // shift down entries to add new one
			high_score[n+1]=high_score[n];
			high_level[n+1]=high_level[n];
			strcpy(high_name[n+1], high_name[n]);
		}
		high_score[i]=newscore;	// setup new entry
		high_level[i]=endlevel;
		high_name[i][0]='\0';
		return i;		// return entry number
	}
	return -1;			// not on high scores
}

// routine to display the high score table on screen
void display_highscores(int hilite)
{
	char s[80];
	int i;

	// first draw the title
	DrawText("Score   Level  Name", TITLEXPOS, TITLEYPOS, TITLECOLOUR);
	for(i=0; i<NUMSCORES; i++) { // loop for scores
		sprintf(s, "%07ld   %02d   %s", \
		 high_score[i], high_level[i], high_name[i]); // create entry
		if(i==hilite) { // if highlighted entry
			DrawText(s, SCOREXPOS, SCOREYPOS+(i*SCOREYDIFF), HIGHCOLOUR);
		} else {
			DrawText(s, SCOREXPOS, SCOREYPOS+(i*SCOREYDIFF), NORMCOLOUR);
		}
	}
}

// routine to check if brick above the one passed, brickno returned
int above(int n)
{
	int t;

	t=n-BricksX;
		if(t<0) return -1;       // if past top, non above
		if(bricks[t]->deleted()) return -1;
		if(bricks[t]->moving()) return -1; // no hit if deleted or moving
		return t;       // else there is a brick above
}

// routine to check brick below current one
int below(int n)
{
	int t;

	t=n+BricksX;
		if(t>NumBricks-1) return -1;     // if below bottom, no brick
		if(bricks[t]->deleted()) return -1;
		if(bricks[t]->moving()) return -1; // no hit if deleted or moving
		return t;       // brick there
}

// check for brick to right
int toright(int n)
{
	int t;

	t=n+1;
		if(n%BricksX == BricksX-1) return -1; // past right so no brick
		if(bricks[t]->deleted()) return -1;
		if(bricks[t]->moving()) return -1; // no hit if deleted or moving
		return t;       // brick there
}

// check for brick to left
int toleft(int n)
{
	int t;

	t=n-1;
		if(n%BricksX == 0) return -1; // past left so no brick
		if(bricks[t]->deleted()) return -1;
		if(bricks[t]->moving()) return -1; // no hit if deleted or moving
		return t;       // brick there
}
