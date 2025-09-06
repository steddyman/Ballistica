// 3DS platform implementation (citro2d) replacing legacy DOS VGA routines.
// Bottom screen (320x240) rendering only.

#include "hardware.hpp"
#ifdef PLATFORM_3DS
#include <3ds.h>
#include <citro2d.h>
#include <array>

// Include embedded atlases (.t3x turned into headers by Makefile)
#include "IMAGE.h"      // main sprite sheet (bricks, bat, ball, etc.)
#include "BREAK.h"      // title / break screen
#include "DESIGNER.h"   // level designer background
#include "INSTRUCT.h"   // instructions screen
// #include "HIGH.h"       // (high score screen atlas not yet ported)

// Sprite index headers
#include "sprite_indexes/image_indices.h"
// Additional index headers (BREAK, DESIGNER, etc.) kept for completeness
#include "sprite_indexes/break_indices.h"
#include "sprite_indexes/designer_indices.h"
#include "sprite_indexes/instruct_indices.h"
#include "sprite_indexes/high_indices.h"

namespace {
	C3D_RenderTarget* g_bottom = nullptr;
	// Core sprite sheets (loaded from embedded memory)
	C2D_SpriteSheet g_sheetImage = nullptr;
	C2D_SpriteSheet g_sheetBreak = nullptr;
	C2D_SpriteSheet g_sheetDesigner = nullptr;
	C2D_SpriteSheet g_sheetInstruct = nullptr;
	C2D_SpriteSheet g_sheetHigh = nullptr; // unused placeholder

	bool loadSheet(C2D_SpriteSheet& out, const u8* data, u32 size) {
		out = C2D_SpriteSheetLoadFromMem(data, size);
		return out != nullptr;
	}
}

bool hw_init() {
	gfxInitDefault();
	if (C3D_Init(C3D_DEFAULT_CMDBUF_SIZE) != 0) return false;
	if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
	C2D_Prepare();
	g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	if(!g_bottom) return false;

	// Load all required sprite sheets from embedded memory
	if(!loadSheet(g_sheetImage, IMAGE_t3x, IMAGE_t3x_size)) return false;
	if(!loadSheet(g_sheetBreak, BREAK_t3x, BREAK_t3x_size)) return false;
	if(!loadSheet(g_sheetDesigner, DESIGNER_t3x, DESIGNER_t3x_size)) return false;
	if(!loadSheet(g_sheetInstruct, INSTRUCT_t3x, INSTRUCT_t3x_size)) return false;
	// High score sheet omitted (not available yet)
	return true;
}

void hw_shutdown() {
	if(g_sheetHigh) C2D_SpriteSheetFree(g_sheetHigh); g_sheetHigh=nullptr;
	if(g_sheetInstruct) C2D_SpriteSheetFree(g_sheetInstruct); g_sheetInstruct=nullptr;
	if(g_sheetDesigner) C2D_SpriteSheetFree(g_sheetDesigner); g_sheetDesigner=nullptr;
	if(g_sheetBreak) C2D_SpriteSheetFree(g_sheetBreak); g_sheetBreak=nullptr;
	if(g_sheetImage) C2D_SpriteSheetFree(g_sheetImage); g_sheetImage=nullptr;
	if(g_bottom) { C2D_TargetDelete(g_bottom); g_bottom=nullptr; }
	C2D_Fini();
	C3D_Fini();
	gfxExit();
}

void hw_poll_input(InputState& out) {
	hidScanInput();
	u32 kHeld = hidKeysHeld();
	touchPosition tp{};
	out.touching = (kHeld & KEY_TOUCH) != 0;
	if(out.touching) {
		hidTouchRead(&tp);
		out.stylusX = tp.px; // 0..319
		out.stylusY = tp.py; // 0..239
	} else {
		out.stylusX = out.stylusY = -1;
	}
	out.fireHeld = (kHeld & KEY_DUP) != 0;
}

void hw_begin_frame() {
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(g_bottom, C2D_Color32(0,0,0,255));
	C2D_SceneBegin(g_bottom);
}

void hw_end_frame() {
	C3D_FrameEnd(0);
}

void hw_draw_sprite(C2D_Image img, float x, float y, float z, float sx, float sy) {
	C2D_DrawImageAt(img, x, y, z, nullptr, sx, sy);
}

// Convenience accessor for default IMAGE sheet
C2D_Image hw_image(int index) {
	if(!g_sheetImage) return C2D_Image{};
	return C2D_SpriteSheetGetImage(g_sheetImage, index);
}

#endif // PLATFORM_3DS

#ifndef PLATFORM_3DS

void Fade2Dark(PalData *Palette, PalData *Palin)
{
	int i, n;
	for(i=0;i<256;i++) {
		Palette->Red[i]=Palin->Red[i]&0x3f;	// Strip of lower 6 bits
		Palette->Green[i]=Palin->Green[i]&0x3f;
		Palette->Blue[i]=Palin->Blue[i]&0x3f;
	}
	for(n=0;n<32;n++) {		// 32 step fade
		WaitRetrace();		// Wait for vertical retrace
		for(i=0;i<256;i++) {	// 256 colour palette
			asm cli;
			outportb(DacWrite, i);	// Select palette register
			outportb(DacData, Palette->Red[i]);	// now send colour
			if(Palette->Red[i] >0) Palette->Red[i]--;
			outportb(DacData, Palette->Green[i]);
			if(Palette->Green[i] >0) Palette->Green[i]--;
			outportb(DacData, Palette->Blue[i]);
			if(Palette->Blue[i] >0) Palette->Blue[i]--;
			asm sti;
		}
	}
	return;
}

// Function to fade from black to current palette colour
void FadeFromDark(PalData *BPalette, PalData *Palin)
{
	int i, n;
	PalData *Palette = new PalData;			// Colour palette

	for(i=0;i<256;i++) {
		Palette->Red[i]=Palin->Red[i]&0x3f;	// Strip of lower 6 bits
		Palette->Green[i]=Palin->Green[i]&0x3f;
		Palette->Blue[i]=Palin->Blue[i]&0x3f;
	}
	for(n=0;n<32;n++) {		// 64 step fade
		WaitRetrace();		// Wait for vertical retrace
		for(i=0;i<256;i++) {	// 256 colour palette
			asm cli;
			outportb(DacWrite, i);	// Select palette register
			outportb(DacData, BPalette->Red[i]);	// now send colour
			if(BPalette->Red[i] < Palette->Red[i]) BPalette->Red[i]++;
			outportb(DacData, BPalette->Green[i]);
			if(BPalette->Green[i] < Palette->Green[i]) BPalette->Green[i]++;
			outportb(DacData, BPalette->Blue[i]);
			if(BPalette->Blue[i] < Palette->Blue[i]) BPalette->Blue[i]++;
			asm sti;
		}
	}
	return;
}

// Function to turn colour palette on screen to grey.  First value passed
// is the start palette number, second is number of registers.
void Palette2Grey(int First, int NumReg)
{
	asm {
		mov	bx,First
		mov	cx,NumReg
		mov	ax,101bh
		int	10h		// Use interupt 10 function
	}
	return;
}

// Function to draw sprite onto the screen
// Values passed are a pointer to the top left corner of the 16 by 16
// pixel area.  X and Y co-ordinates are also passed
void DrawSprite(int sx, int sy, void far *)
{
	asm {
		push	ds		// Save segment registers
		push	si
		push	es
		push	di
		mov	ax,0a000h
		mov	es,ax
		mov	ax,SCRWIDTH
		mov	bx,sy
		mul	bx
		add	ax,sx	// Add on x offset
		mov	di,ax	// es:di now points to destination for sprite on scr
		mov	ax, [bp+12]
		mov	ds,ax
		mov	si, [bp+10]	// ds:si points to start of sprite
		mov	cx,SPRWIDTH	// load sprite width
		mov	bx,SPRDEPTH
		mov	ah,0ffh	// set AH for mask
	}
	LOOPSTART:
	asm {
		lodsb		// load start byte
		and	al,ah
		jz	SKIPOVER	// don't store if zero byte
		stosb		// store to destination byte
		loop	LOOPSTART	// branch if not at the end
		jmp	LINEDONE	// jump to line finished routine

	}
	SKIPOVER:
	asm {
		inc	di		// increment desti
		loop LOOPSTART	// branch back into transfer loop
	}
	LINEDONE:			// end of line reached
	asm {
		add	si,ADDON	// add line addon to start and end
		add	di,ADDON
		mov	cx,SPRWIDTH	// reload width register
		dec	bx
		jnz	LOOPSTART	// Back to transfer if not at bottom of sprite
		pop	di
		pop	es
		pop	si
		pop	ds		// restore registers
	}
	return;			// all done, now return
}

// Function to save the area underlying where a sprite is about to
// go to a buffer.  x and y co-ordinates of sprite postion on screen along
// with a pointer to the buffer are passed to the routine
void SaveBack(int sx, int sy, void far *)
{
	asm {
		push	ds
		push	si
		push	es
		push	di		// save the segment registers on stack
		mov	ax,0a000h
		mov	ds,ax
		mov	ax,SCRWIDTH
		mov	bx,sy
		mul	bx
		add	ax,sx
		mov	si,ax	// ds:si now points to sprite on screen
		mov	ax,[bp+12]
		mov	es,ax
		mov	di,[bp+10]	// es:di points to sprite buffer
		mov	cx,SPRWIDTH	// set width of the sprite
		mov	bx,SPRDEPTH	// set depth of the sprite
	}
	SAVESTART:
	asm {
		rep	movsb		// transfer sprite line
		add	si,ADDON		// addon screen ofset
		mov	cx,SPRWIDTH
		dec	bx
		jnz	SAVESTART		// Wait until complete sprite saved
		pop	di
		pop	es
		pop	si
		pop	ds
	}
	return;
}

// Function to restore background to the screen from the sprite buffer
// it was saved to.  Parameters passed are x and y co-ordinates on the
// screen to transfer the data back into
void RestoreBack(int sx, int sy, void far *)
{
	asm {
		push	ds
		push	si
		push	es
		push	di		// save the segment registers on stack
		mov	ax,0a000h
		mov	es,ax
		mov	ax,SCRWIDTH
		mov	bx,sy
		mul	bx
		add	ax,sx
		mov	di,ax	// es:di now points to area on screen
		mov	ax,[bp+12]
		mov	ds,ax
		mov	si,[bp+10]	// ds:si points to sprite buffer
		mov	cx,SPRWIDTH	// set width of the sprite
		mov	bx,SPRDEPTH	// set depth of the sprite
	}
	SAVESTART:
	asm {
		rep	movsb		// transfer sprite line
		add	di,ADDON		// addon screen ofset
		mov	cx,SPRWIDTH
		dec	bx
		jnz	SAVESTART		// Wait until complete sprite saved
		pop	di
		pop	es
		pop	si
		pop	ds
	}
	return;
}

// Function to read both joystick x and y values and button status
// The global Stick structure is filled in by the routine
void ReadJoystick(int Joy)
{
	int i;
	JoyPos[Joy].current_x=ReadPot((Joy*2));
	JoyPos[Joy].current_y=ReadPot((Joy*2)+1);
	JoyPos[Joy].button_1=ReadFire(Joy)&0x01?0xff:0x00;
	JoyPos[Joy].button_2=ReadFire(Joy)&0x02?0xff:0x00;
	return;
}
// Assembly language routine to read joystick fire buttons
char ReadFire(int JoyNum)
{
	char Store;

	asm {
		mov	dx,0201h
		in	al,dx
		mov	Store,al	// Read and store button status
	}
	if(JoyNum==0) {			// If joystick 0
		Store=Store>>4;		// Shift down 4 bits
	} else {
		Store=Store>>6;
	}
	return Store&0x03;
}
// Assembly language routine to read a pot number
uint ReadPot(char PotNum)
{
	int	value;

	asm {
		mov	ax,1
		mov	cl,PotNum
		cmp	cl,0
		jz	noshift
		shl	ax,cl	// Move mask to correct value
	}
	noshift:
	asm {
		mov	ah,al	// copy to new position
		mov	dx,0201h	// set up address to joystick port
		cli
		xor	al,al
		out	43h,al	// latch value
		in	al,40h
		mov	bl,al
		in	al,40h
		mov	bh,al	// Get current value
		jnz	noskip	// dummy branch
	}
	noskip:
	asm {
		xor	al,al
		out	43h,al
		in	al,40h
		mov	cl,al
		in	al,40h
		mov	ch,al	// Get value again to determine delay in getting value
		sub	bx,cx	// bx is delay diff, cx is start of timer
		push	bx		// stack equals final value to subtract
		push	cx		// and start of timer loop
		mov	bx,cx
		sub	bx,2000h	// Max wait before no joystick detected in bx
		mov	al,ah	// reload the register
		out	dx,al
		test	cx,cx
		jl	loop3	// Other type of loop for signed value
	}
	loop2:			// Loop to test joystick level
	asm {
		in	al,dx
		test	ah,al
		jz	found	// wait for a change
		xor	al,al
		out	43h,al
		in	al,40h
		mov	cl,al
		in	al,40h
		mov	ch,al
		cmp	cx,bx
		jge	loop2	// max sure max value not return
		sti
		pop	ax
		pop	ax
		xor	ax,ax
		jmp	done			// return value 0 if a timeout
	}
	loop3:			// Loop to test joystick level
	asm {
		in	al,dx
		test	ah,al
		jz	found	// wait for a change
		xor	al,al
		out	43h,al
		in	al,40h
		mov	cl,al
		in	al,40h
		mov	ch,al
		cmp	cx,bx
		jae	loop3	// max sure max value not return
		sti
		pop	ax
		pop	ax
		xor	ax,ax
		jmp done			// return value 0 if a timeout
	}
	found:			// Joystick direction indicated
	asm {
		xor	al,al
		out	43h,al
		in	al,40h
		mov	cl,al
		in	al,40h
		mov	ch,al		// Re-read timer value
		pop	ax		// Get start of timer value
		sub	ax,cx	// find difference
		pop	cx		// Get minimum loop value
		sub	ax,cx	// also subtract
	}
	done:
	asm {
		mov	value,ax
		mov	cx,200h		// Delay for end of joyroutine
		mov	ax,PotNum
		cmp	ax,2
		jae	endwait2		// Check which ports to wait on
	}
	endwait:
	asm {
		in	al,dx
		and	al,03h
		cmp	al,0
		loopnz	endwait
		jmp	nowret
	}
	endwait2:
	asm {
		in	al,dx
		and	al,0ch
		cmp	al,0
		loopnz	endwait2
	}
	nowret:
	asm	sti
	return value;
}

// Routine to setup pointers to font bitmap information
void ROMFont(void)
{
	asm	{
		push	es
		push	bp		// save used registers
		mov	ah,11h
		mov	al,23h	// Video functionto load 8x8 graphics font
		mov	bl,2		// 25 character deep font
		mov	dl,0
		int	10h		// setup video font pointer
		mov	ah,11h
		mov	al,30h	// Video function return char gen info
		mov	bh,3		// select 8x8 lower character set
		int	10h		// and get information
		mov	FONTOFF,bp	// Save offset
		mov	ax,es
		mov	FONTSEG,ax	// Save segment
		pop	bp
		pop	es		// restore registers
	}
	return;
}

// Routine to print text to vga13 graphics screen in transparent mode
// parameters passed are a string, x and y co-ordinates and a colour
void DrawText(char far *string, int x, int y, char colour)
{
	int fonts, fonto;
	asm {
		push	ds
		push	es

		mov	ax,FONTSEG
		mov	fonts,ax
		mov	ax,FONTOFF
		mov	fonto,ax		// save font pointer onto stack
		mov	ax,0a000h		// setup destination segment to screen
		mov	es,ax
		mov	ax,SCRWIDTH	// workout the offset for start of chars
		mov	bx,y			// by 320*y+x
		mul	bx
		add	ax,x			// add on x
		mov	di,ax		// and store to index
		push	di			// and save
		mov	bx,SCRWIDTH*SCRDEPTH	// setup pointer to temp string store

		lds	si,string		// now at start of text
	}
	LOOP1:
	asm {
		mov	al,ds:si		// get first byte of text
		mov	es:bx,al		// and store
		cmp	al,0
		jz	COPIED		// check if last byte and jump
		inc	si
		inc	bx
		jmp	LOOP1
	}
	COPIED:
	asm {
		mov	bx,320*200	// Set bx back to text offset
		mov	ax,fonts		// Get font segment pointer
		mov	ds,ax		// and store
		mov	cx,0			// Number of rows in font counter
		mov	dh,colour

		// Now main loop starts retreiving character from the undisplayed
		// area of screen memory and storing them to the screen.

	}
	NEXTCHAR:
	asm	{
		xor	ah,ah
		mov	al,es:bx		// get first character
		cmp	al,0
		jz	EOS			// check for end of string
		mov	si,fonto		// and get offset
		shl	ax,1
		shl	ax,1
		shl	ax,1			// multiply by eight for offset to font table
		add	ax,cx
		add	si,ax		// add onto to get character source
		lodsb			// now read character byte
		mov	dl,80h		// and setup dl for testing and shifting
	}
	NEXTBIT:
	asm	{
		test	al,dl		// and together to see if bit needs setting
		jz	NOSET		// branch if it doesn't
		mov	es:di,dh		// store colour
	}
	NOSET:
	asm {
		inc	di			// move to next byte on screen
		shr	dl,1			// shift round mask bit
		jnc	NEXTBIT		// and check if all byte done (not)
		inc	bx
		jmp	NEXTCHAR		// back up for next character
	}
	EOS:
	asm {
		inc	cx			// increment line counter
		cmp	cx,8			// and see if finsihed
		je	DONE			// jump if so
		pop	di
		push	di			// retrieve original index
		mov	ax,SCRWIDTH
		mul	cx			// get addon for start of line
		add	di,ax		// and add to index
		mov	bx,320*200	// reload pointer to start of font
		mov	dh,colour
		jmp	NEXTCHAR		// and continue
	}
	DONE:				// reach here when all completed
	asm	{
		pop	di
		pop	es
		pop	ds			// retreive all segment registers
	}
	return;
}

// Function to clear a mode 13 screen
void ClearScreen(void)
{
	char far *scrpointer=(char far *)0xa0000000L;
	unsigned i;

	for(i=0;i<64000;i++) *(scrpointer+i)=(char)0;
}

// Function to clear a box on the screen to a set colour
void ClearBox(int x, int y, int width, int height, char colour)
{
	asm {
		push	es
		push	di
		mov	ax,0a000h
		mov	es,ax
		mov	ax,SCRWIDTH
		mov	bx,y
		mul	bx
		add	ax,x			// Work out start x-y address
		mov	di,ax
		mov	dx,height
	}
	NEXTLINE:
	asm {
		mov	cx,width
		mov	al,colour
		rep	stosb		// clear one line
		mov	ax,SCRWIDTH
		sub	ax,width		// workout add on value
		add	di,ax		// and add onto di
		dec	dx
		jnz	NEXTLINE		// branch for next line
		pop	di
		pop	es
	}
	return;
}

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

#endif // !PLATFORM_3DS