#include "support.hpp"
#include "video.hpp"
#include <process.h>
#include <iostream.h>

int SinDir[360];		// Sin table for ball objects
int CosDir[360];		// Cosine table for ball objects

unsigned int PageOffsets[] = {
	0, 16000, 32000, 48000 };
extern char far *backptr;
void interrupt (*oldhandler)(__CPPARGS);
volatile int GlobalDelay = 9999;		// delay for sound switch of
volatile int GlobalTone=0;
volatile int SoundFlag=0;
volatile int mrange;				// mouse range indicate

block :: block(int w, int h)
{
	height=h; width=w;
	hits=1; lasthit=0;
	del=killed=0; falldel=1;
	explodenum=FAKEANI;
}

void ballblock :: settype(int tp, unsigned char far *pointer)
{
	type=tp;			// setup srpite type no
	sprdata=pointer;
}

void block :: settype(int tp, unsigned char far *pointer)
{
	type=tp;			// setup srpite type no
	sprdata=pointer;
	if(type==NB) {
		height=width=0;	// no block setup
	}
	sidedir=0;			// default sideways direction is zero
	switch(type) {
		case NB:		// No brick is already deleted
			del=1;
			break;
		case T5:		// Five hits brick set hit delay
			hits=5;
			break;
		case SS:		// moving bricks mark as such
		case SF:
			moves=1;
			sidedir=-1;
		default:
			break;
	}
	if(type==ID || type==SF) destruct=1;	// setup indestructible type
	else destruct=0;
	if(type==IF || type==IS) solid=0;
	else solid=1;
}

block :: ~block()
{
}

int block :: process_hit()
{
	if(type==ID || type==SF) return 0;	// if an indestructible brick
	if(type==BO) return 0;	// bomb brick is a kind of indestructible brick
	if(type==IS || type==IF || type==AB || type==MB) {
		dele();
		return 0;
	}
	if(type==SS) {
		dele();
		return 0;
	}

	if(type==T5) {	// five hit brick
		if(hits) {	// if still hits left
			hits--;
		} else {	// no hits left
			dele();	// mark as deleted
		}
		return 0;
	}
	if(type>=LB) {		// if block should move?
		markmove();		// then start it moving
		lasthit=1;		// mark as just hit
		lasthitx=oldx;
		lasthity=oldy;
		return 1;
	} else {
		dele();
	}
	return 0;
}

// Routine to process a surrounding brick affected by a bomb
int block :: process_explosion()
{
	if(type==ID || type==SF || type==BO) { // Indestructible or moving indestructible brick
		return 0;	// No action, indestructible
	} else if(type<=RB || (type>=B1 && type <=B5) || type==AB \
		   || type==MB || type==T5 || type==IS || type==IF \
		   || type==AB || type==OF || type==ON || type==SS) { // none falling brick
		   dele();	// destroyed but activated externally
		   return 0;
	} else {	// brick is going to fall, but not with an explosion
		markmove();		// then start it moving
		lasthit=1;		// mark as just hit
		lasthitx=oldx;
		lasthity=oldy;
		return 1;
	}
}

int block :: move(void)		// Move down a line
{
	if(type==SF || type==SS) return FALSE;	// don't move horizontal bricks
	if(type==F1) {
		if(falldel--==0) {
			falldel=1;
			y++;
		}
	} else y++;
	return TRUE;
}

void block :: moveside(void)	// move block sideway in current direction
{
	if(lasthit) lasthit=0;
	else if(falldel-- == 0) {
		if(sidedir>0) x++;
		else if(sidedir<0) x--;
		falldel=1;
	}
	return;
}

void block :: markmove(void) 	// mark as moving
{
	moves=1;
	falldel=1;
	solid=0;	// no longer collide with falling blocks
}

void block :: resize(int w, int h, unsigned char far *pointer)
{
	width=w; height=h;
	sprdata=pointer;
}


ballblock :: ballblock(int w, int h)
{
	height=h; width=w;
	speed=10;spin=0; // 1 pixel movements and no spin
	magx=magy=0;
	killed=del=0;
}

void init_floattable(void)
{
	float PiOver180=float(M_PI/180);
	float xf, yf, tf;	// tempoaray float varibles
	for(int i=0;i<360;i++) {	// loop for all degrees
		xf=cos(float(i)*PiOver180); // Compute Cosines (x)
		tf=xf/1.0;	// workout what percentage
		CosDir[i]=(int)float(StepsPixel)*tf;	// convert to integer range
		yf=sin(float(i)*PiOver180); // Compute Sines (y)
		tf=yf/1.0;	// workout what percentage of 1
		SinDir[i]=(int)float(StepsPixel)*tf;	// convert to integer range
	}
	CosDir[0]=SinDir[90]=StepsPixel;	// set to 1 pixel
	CosDir[90]=SinDir[0]=0;
	CosDir[180]=SinDir[270]=NegStepsPixel;	// set to -1 pixel
	CosDir[270]=SinDir[180]=0;
}

void ballblock :: sitonbat(block *bat)
{
	x=bat->x+((bat->getwidth()/2)-(width/2));
	y=bat->y-height;
	colx=x*StepsPixel;
	coly=y*StepsPixel;
	ang=45;
	int a=(ang+270)%360;	// convert co-ordinates
	int f=CosDir[a];
	if(f<0) xdir=-1;
	else if(f>0) xdir=1;
	else xdir=0;
	f=SinDir[a];
	if(f<0) ydir=-1;
	else if(f>0) ydir=1;
	else ydir=0;
}

void ballblock :: position(int sx, int sy)
{
	x=oldx=sx;y=oldy=sy;
	ox=sx; oy=sy;
	colx=x*StepsPixel;
	coly=y*StepsPixel;
	magx=magy=1;
	ang=35;
	int a=(ang+270)%360;	// convert co-ordinates
	int f=CosDir[a];
	if(f<0) xdir=-1;
	else if(f>0) xdir=1;
	else xdir=0;
	f=SinDir[a];
	if(f<0) ydir=-1;
	else if(f>0) ydir=1;
	else ydir=0;
}

void block :: dele()	// mark block as deleted
{
	del=1;
}

// check for a collision with the bat and a falling block
int block :: collision(const block &blck)
{
	int endx=x+width;
	int endy=y+height;
	int bendx=blck.x+blck.width;

	// check if ball is hitting right side of block
	if(!((bendx<x) || (blck.x>endx))) {
		if(endy==blck.y) return 1;	// Collision with top of bat
	}
	return 0;
}

// check for a collision with the bat
int block :: collision_bat(const ballblock &ball)
{
	int bendx=ball.x+ball.width-1;
	int bendy=ball.y+ball.height-1;
	int endx=x+width-1;
	int endy=y+height-1;

	if(ball.ydir<0) return FALSE; // no collision if ball moving up
	if(bendy>=y && (bendy-2)<=endy) { // within Y range of ball
		if(bendx>=x && ball.x<=endx) return TRUE;
	}
	return FALSE;
}

// check if mouse if over block
int block :: collision_mouse(const block &mouse)
{
	int bendx=mouse.x+mouse.width-1;
	int bendy=mouse.y+mouse.height-1;
	int endx=x+width-1;
	int endy=y+height-1;

	if(bendy>=y && (bendy-2)<=endy) { // within Y range of ball
		if(bendx>=x && mouse.x<=endx) return TRUE;
	}
	return FALSE;
}

// Check if passed boundaries will cross block position
int block :: collision(const ballblock &ball)
{
	int bx=ball.x+1; int by=ball.y+1;
	int endx=x+width-1;
	int endy=y+height-1;
	int xdir=ball.xdir;
	int ydir=ball.ydir;		// Preload all registers for speed
	int hitx, hity;
	hitx=hity=0;

	if(xdir<0) bx--;
	else if(xdir>0) bx++;
	if(ydir<0) by--;
	else if(ydir>0) by++;

	if(!((by<y) || (by>endy))) {
		if(endx==bx) hitx=1;       // right hit
		else if(x==bx) hitx=2;  // left hit
	}
	if(!((bx<x) || (bx>endx))) {
		if(y==by) hity=1; // top hit
		else if(endy==by) hity=2;      // bottom hit
	}

	if(!(hitx+hity)) return 0;		// no hit
	if(xdir==0 && ydir==0) return 0;

	if(ydir<0) {			// ball moving up
		if(xdir<0) {        // and left
			if(hitx==1 && hity==2) return LR;
			if(hitx==1) return RIG;
			if(hity==2) return BOT;
		} else if(xdir>0) {	// xdir >0 - right
			if(hitx==2 && hity==2) return LL;
			if(hitx==2) return LEF;
			if(hity==2) return BOT;
		} else {	// straight up
			if(hity==2) return BOT;
		}
	} else if(ydir>0) {		// ball moving down
		if(xdir<0) {		// and left
			if(hitx==1 && hity==1) return UR;
			if(hitx==1) return RIG;
			if(hity==1) return TOP;
		} else if(xdir>0) {	// and right
			if(hitx==2 && hity==1) return UL;
			if(hitx==2) return LEF;
			if(hity==1) return TOP;
		} else {	// straight down
			if(hity==1) return TOP;
		}
	} else {	// moving along a straight horizontal path
		if(xdir<0) {	// Moving straight left
			if(hitx==1) return RIG;
		} else {		// Moving straight right
			if(hitx==2) return LEF;
		}
	}
	return 0;	// no hit if reaches here
}
// Check if passed boundaries will cross block position
int block :: moving_hit(const ballblock &ball)
{
	int bx=ball.x; int by=ball.y;
	int endx=x+width-1;
	int endy=y+height-1;
	int xdir=ball.xdir;
	int ydir=ball.ydir;		// Preload all registers for speed
	int hitx, hity;
	hitx=hity=0;

	if(!((by<y) || (by>endy))) {
		if(endx==bx) hitx=1;       // right hit
		else if(x==bx) hitx=2;  // left hit
	}
	if(!((bx<x) || (bx>endx))) {
		if(y==by) hity=1; // top hit
		else if(endy==by) hity=2;      // bottom hit
	}

	if(!(hitx+hity)) return 0;		// no hit
	if(xdir==0 && ydir==0) return 0;

	if(hitx==1 && hity==2) return LR;
	if(hitx==2 && hity==2) return LL;
	if(hitx==1 && hity==1) return UR;
	if(hitx==2 && hity==1) return UL;
	if(hitx==1) return RIG;
	if(hitx==2) return LEF;
	if(hity==1) return TOP;
	if(hity==2) return BOT;
	return 0;	// no hit if reaches here
}

void block :: draw()                         // draw block function
{
	if(height==0 || width==0) return;
	if(y+height>=SCRDEPTH) {
		blitSprite(x, y, SCRDEPTH-y, width, sprdata);
	} else {
		if(type<EB) blitBrick(x, y, sprdata);	// if brick use fast draw routine
		else blitSprite(x, y, height, width, sprdata);
	}
}

void block :: erase()                        // draw block function
{
	if(oldy>=SCRDEPTH) return;
	if(height && width) {
		if(oldy+height>SCRDEPTH) {
			eraseSprite(oldx, oldy, SCRDEPTH-oldy, width, backptr);
		} else {
			if(type<EB) eraseBrick(oldx, oldy, backptr);
			else eraseSprite(oldx, oldy, height, width, backptr);
		}
	}
	oldx=x; oldy=y;
}

void block :: drawback()	// used to draw bricks into background
{
	if(height==0 || width==0) return;
	blitBrickBack(x, y, sprdata, backptr);
}

// slow routine to save under sprite
void block :: save(char far *buffer)
{
	unsigned char far *spointer=(unsigned char far *)0xa0000000L;
	int i=0;
	for(int ny=0;ny<height;ny++) {
		for(int nx=0; nx<width;nx++) {
			buffer[i++]=*(spointer+((y+ny)*320)+(x+nx));
		}
	}
}

// slow routine to restore sprite from a buffer
void block :: restore(char far *buffer)
{
	unsigned char far *spointer=(unsigned char far *)0xa0000000L;
	int i=0;
	for(int ny=0;ny<height;ny++) {
		for(int nx=0; nx<width;nx++) {
			*(spointer+((y+ny)*320)+(x+nx))=buffer[i++];
		}
	}
}

void ballblock :: draw()                         // draw ball function
{
	if(height==0 || width==0) return;
	if(y+height>=SCRDEPTH) blitSprite(x, y, SCRDEPTH-y, width, sprdata);
	else blitBall(x, y, sprdata);	// if fits on screen use fast draw
}

void ballblock :: erase()                        // draw block function
{
	if(oldy>=SCRDEPTH) return;
	if(oldy+height>SCRDEPTH) {
		eraseSprite(oldx, oldy, SCRDEPTH-oldy, width, backptr);
	} else {
		eraseBall(oldx, oldy, backptr);
	}
	oldx=x; oldy=y;
}


void block :: drawmask()
{
	if(height==0 || width==0) return;
	if(y>=SCRDEPTH) return;
	if(y+height>SCRDEPTH) {
		blitMask(x, y,SCRDEPTH-y, width, id, maskbuffer);
	} else {
		blitMask(x, y, height, width, id, maskbuffer);
	}
}

void block :: erasemask()                        // draw block function
{
	if(oy>=SCRDEPTH) return;
	if(height && width) {
		if(oy+height>SCRDEPTH) {
			blitMask(ox, oy, \
					 SCRDEPTH-oy, width, 0, maskbuffer);
		} else {
			blitMask(ox, oy, \
					 height, width, 0, maskbuffer);
		}
	}
	ox=x; oy=y;
}

void ballblock :: drawmask()
{
	setMaskPixel(x+1, y+1, id);
}

void ballblock :: erasemask()                        // draw block function
{
	setMaskPixel(ox+1, oy+1, 0);
	ox=x; oy=y;
}

void block :: droperasemask()                        // draw block function
{
	if(y>=SCRDEPTH) return;
	if(height && width) {
		if(lasthity+height>SCRDEPTH) {
			blitMask(lasthitx, lasthity, \
					 SCRDEPTH-lasthity, width, 0, maskbuffer);
		} else {
			blitMask(lasthitx, lasthity, \
					 height, width, 0, maskbuffer);
		}
	}
}

void ballblock :: updatepos(int v)	// Update ball position function
{
	int a=(ang+270)%360;	// convert from real to display co-ordinates
	if(magx) {
		colx=colx+((CosDir[a]*v)/10);
		x=colx/StepsPixel;
	} else magx=1;
	if(magy) {    // was y
		coly=coly+((SinDir[a]*v)/10);
		y=coly/StepsPixel;
	} else magy=1;
}

void ballblock :: simplecol(int side)	// calculate simple collision
{
	DoBeep(900,8);
	if(side==1 || side==3) {	// Vertical collision?
		ang=(540-ang+spin)%360;
		magy=0; magx=1;
	} else if(side==2 || side==4) {                   	// Horizontal collision
		ang=(360-ang+spin)%360;
		magx=0; magy=1;
	} else {		// diagonal hit
		ang=(ang+180)%360;
		magx=magy=1;// was 1
	}
	if(random(12)==5) {	// random mod of the angle
		ang=ang+1;
		ang=ang%360;
	}
	int a=(ang+270)%360;	// convert co-ordinates
	int f=CosDir[a];
	if(f<0) xdir=-1;
	else if(f>0) xdir=1;
	else xdir=0;
	f=SinDir[a];
	if(f<0) ydir=-1;
	else if(f>0) ydir=1;
	else ydir=0;

}

// Calculate collision with bat to modify angle
void ballblock :: batcol(int s)
{
	int	st;		// temporary speed
	DoBeep(400,15);
	magy=1;
	st=s*5;
	if(st>mrange) st=mrange;
	else if(st<-mrange) st=-mrange;	// limit speed to range +-

	ang=((540-ang)+st)%360;	// Compute angle of reflection with speed
	if(xdir<0 && (ang<295 && ang>=180)) ang=295;
	else if(xdir>0 && (ang>55 && ang<=180)) ang=55;	// limit acute angles
	else if(ang<10 && ang>=0) ang=10;
	else if(ang>350 && ang<=360) ang=350;
	int a=(ang+270)%360;	// convert co-ordinates
	int f=CosDir[a];
	if(f<0) xdir=-1;
	else if(f>0) xdir=1;
	else xdir=0;
	f=SinDir[a];
	if(f<0) ydir=-1;
	else if(f>0) ydir=1;
	else ydir=0;

}

// function to set ball direction
void ballblock :: setdir(int d)
{
	ang=d;
	int a=(ang+270)%360;	// convert co-ordinates
	int f=CosDir[a];
	if(f<0) xdir=-1;
	else if(f>0) xdir=1;
	else xdir=0;
	f=SinDir[a];
	if(f<0) ydir=-1;
	else if(f>0) ydir=1;
	else ydir=0;
}


// constructor for integer array type
intarray :: intarray(int n)
{
	ceiling=n;			// set maximum number of entries
	buffer=new int[n];	// allocate memory for buffer
	numentries=iterator=0;
}

// destructor to deallocate memory for array
intarray :: ~intarray()
{
	delete buffer;		// deallocate memory
}

// add function to add an element to the array
void intarray :: add(int v)
{
	if(numentries==ceiling) return;	// no more space for entries
	buffer[numentries]=v;
	numentries++;
}

// function to average values of the array
int intarray :: average(void)
{
	long sum=0;

	for(iterator=0;iterator<numentries;iterator++) {
		sum+=buffer[iterator];	// add entry to sum
	}
	return int(sum/long(numentries));	// return the average
}

// function to remove a value from the array
void intarray :: remove(int n)
{
	if(n>=numentries) return;	// if past end of array return
	if(numentries==0) return;	// no entries to remove
	if(n!=numentries-1) {		// if not last element of array
		for(iterator=n+1; iterator<=numentries-1; iterator++) {
			buffer[iterator-1]=buffer[iterator];	// shift down array
		}
	}
	numentries--;	// decrease number of entries in array
}

// function to find highest value in array, not over the limit specified
int intarray :: highest(int limit)
{
	int max=0;
	int temp=0, pos=0;

	for(iterator=0; iterator<numentries; iterator++) {
		temp=buffer[iterator];	// get current value
		if(temp<0) temp=-temp;	// convert negative values to positive
		if(limit && temp>limit) temp=limit;
		// check for highest value within limits
		if(temp>max) {
			max=temp;
			pos=iterator;
		}
	}
	return buffer[pos];
}

// constructor function for text class
text :: text(char far *string, int sflag=0)
{
	textlen=strlen(string);
	textdata=new char[textlen+1];	// allocate memory for string
	strcpy(textdata, string);		// copy string to array
	height=CHARYSIZE;
	width=CHARXSIZE*textlen;		// width of string
	shadow=sflag;
	if(shadow) {		// if a shadow requested
		height+=2;		// increase height by 2 pixels
		width+=2;		// increase width by 2 pixels
	}
	background=new char[height*width];	// allocate mem for background store
//	ROMFont();			// setup ROM font in video memory
	colour=15;			// default colour
	x=y=0;				// set default co-ordinates
}

// function to change the current setting of the string
void text :: setstring(char far *string)
{
	if(strlen(string)==textlen) {	// if length of string the same
		strcpy(textdata, string);	// just copy over text
	} else {						// different length
		delete textdata;			// delete old memory store
		delete background;			// delete old size for background;
		textlen=strlen(string);
		textdata=new char[textlen+1];	// allocate memory for string
		strcpy(textdata, string);		// copy string to array
		width=CHARXSIZE*textlen;		// width of string
		height=CHARYSIZE;
		if(shadow) {		// if a shadow requested
			height+=2;		// increase height by 2 pixels
			width+=2;		// increase width by 2 pixels
		}
		background=new char[height*width];	// allocate mem for background store
	}
}

// function to erase the text from the screen
void text :: erase(void)
{
	if(textlen) {
		eraseSprite(x, y, height, width, backptr);
	}
}

// function to draw text on the screen
void text :: draw(void)
{
	// the draw shadow if required
	if(textlen) {
		if(shadow) DrawText(textdata, x+2, y+2, 0);	// to LR and black
		DrawText(textdata, x, y, colour);
	}
}

void SetVMode(char mode)    // Function to set video mode
{
	asm {
	mov ah,0            // function 0 = set video mode
	mov al,mode         // to the passed mode
	int 10h
	}
}

/* void WaitRetrace(void)		// Wait for Vertical retrace
{
	asm mov     dx,03dah
retw1:	asm {
		in      al,dx
		test    al,08h
		jz      retw1
	}    retw2:	asm {
		in	al,dx
		test	al,08h
		jnz	retw2
	}
}  */


// Function to reset mouse and hide the cursor
// returns 0 if OK 1 if failed
int MouseReset(void)
{
	int retval;

	asm {
		mov	ax,21h			// software reset first
		int	33h
		mov     ax,0                    //Mouse reset code
		int     33h                    //Perform mouse interupt
		cmp     ax,-1                   //check for ok
		jne     nomous                  //branch if mouse no found
		push    ax
		mov     ax,2                    //Hide cursor code
		int     33h
		pop     ax
	}
nomous: asm {
		mov	retval,ax
	}
	return retval;
}

// Following routine will read the X motion counters and return
// the value as an int
int MouseMotionX(void)
{
	int retval;

	asm {
		mov     ax,0bh             //Mouse motion enquiry code
		int     33h
		mov     retval,cx          //Store motion for return value
	}
	return retval;
}

// Following routine will read the Y motion counters and return
// the value as an int
int MouseMotionY(void)
{
	int retval;

	asm {
		mov     ax,0bh             //Mouse motion enquiry code
		int     33h
		mov     retval,dx          //Store motion for return value
	}
	return retval;
}

// Routine to read x and y counters simultaneously and store
// the values in motionx and montiony which are global variables
// value returned is the button status
int ReadMouse(int &x, int &y)
{
	int retval;
	int ix, iy;

	asm {
		mov		ax,0bh
		int		33h
		mov		ix,cx
		mov		iy,dx
		mov		ax,03h
		int		33h
		mov     retval,bx
	}
	x=ix; y=iy;
	return retval;
}

// The following routine will detect a button press
// the button status is return in ax as an int
int MouseButton(void)
{
	int retval;

	asm {
		mov     ax,03h              //Get button status code
		int     33h
		mov     retval,bx           //Put button status into ax
	}
	return retval;
}

void DoBeep(int tone, int del)	// Make a sound thru the speaker
{
	GlobalDelay=del>>2;	// reset global delay value to del/4.
	GlobalTone=tone;
	SoundFlag=1;
}

void NoBeep(void)	// turn off any pending sounds
{
	disable();
	GlobalDelay=0;
	SoundFlag=0;
	nosound();
	enable();
}

void interrupt timerhandler(__CPPARGS)	// timer interrupt routine
{
	if(SoundFlag) {
		sound(GlobalTone);	// turn on sound
		SoundFlag=0;		// mark no jobs pending
	}
	if(GlobalDelay) {			// if a sound currently running
		GlobalDelay--;
		if(!GlobalDelay) nosound();	// if now hit zero, turn off speaker
	}
	oldhandler();				// call old interrupt handler
}

void SetupTimer(void)			// setup timer interrupt routine
{
	disable();							// disable interrupts
	oldhandler = getvect(TimerInt); 	// save old interrupt
	setvect(TimerInt, timerhandler);	// setup pointer to new routine
	GlobalDelay=0;
	enable();							// re-enable interrupts
	return;
}

void RestoreTimer(void)			// restore timer interrupt routine
{
	disable();							// disable interrupts
	setvect(TimerInt, oldhandler);		// restore the old handler
	enable();							// re-enable interrupts
}

void errhandler(char far *errtext, int error)		// global error handling routine
{
	if(GlobalDelay!=9999) RestoreTimer();// if timer has been started, restore
	ProgramTimer0(0);
	SetVMode(0x03);				// Change to text mode
	cout << "*** FATAL ERROR CODE " << error << " ***" << endl;
	cout << errtext << endl;
	cout << "Application terminated." << endl;
	exit(1);					// return to DOS
}

void ProgramTimer0(int val)			// used to set time interval for timer 0
{
	disable();
	outportb(0x43, 0x36);	// Code to program timer 0 to mode 3
	outportb(0x40, char(val & 0xff));			// low byte
	outportb(0x40, char((val & 0xff00)>>8));	// high byte
	enable();				// re-enable interupts
}