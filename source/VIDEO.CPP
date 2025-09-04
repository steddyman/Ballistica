/*
	GRAPHICS FUNCTIONS FOR GAME SOURCE FILE
	---------------------------------------

	Creation Date:	24/01/93
	Author:		Stephen Eddy

	Revision History:


--------------------------------------------------------------------------
*/

#include "video.hpp"
#include <dos.h>
#include <string.h>
#include <mem.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <alloc.h>
#include <iostream.h>
#pragma inline

// Global declarations
enum control { Joy1, Joy2, Key1, Key2 } P1Input, P2Input;
int CurrentLevel;
int P1Lives, P2Lives;
struct JoystickPosition JoyPos[2];
struct JoystickCalibrate JoyCal[2];
struct KeyCodes KeyInput[2];
uint FONTSEG;
uint FONTOFF;			// pointer for font data

extern int DisplayedPage, NonDisplayedPage;
extern unsigned int PageOffsets[];
// Function Definitions

// SetColour : Set Palette register to colours specified
void SetColour(char pal_register, char red_val, char green_val, char blue_val)
{
	asm cli;	// Disable interupts
	outportb(DacWrite, pal_register);	// Setup Palette register
	outportb(DacData, red_val);
	outportb(DacData, green_val);
	outportb(DacData, blue_val);
	asm sti;	// Enable interupts
	return;
}

// Function to wait for vertical retrace interupt

void WaitRetrace(void)
{
	asm mov dx,InputStatus;
	WAITVBI1:
	asm {
		in	al,dx
		and	al,VbiBit		// Check for Retrace signal
		jnz	WAITVBI1
	}
	WAITVBI2:
	asm {
		in	al,dx
		and	al,VbiBit
		jz	WAITVBI2		// Test Retrace signal again
	}
	return;
}

// Function to switch to VGA mode 13, 320x200 256 colours
void GraphicsMode(void)
{
	asm {
		xor	ah,ah
		mov	al,13h		// Mode 13
		int	10h			// Video interupt
	}
}

// Function to set mode to 4 plane 320x200 256 colour mode (modeX)
void SetModeX(void)
{
	asm {
		xor	ah,ah
		mov	al,13h		// Mode 13
		int	10h			// Video interupt
		push es
		push di
		cli				// disable interupts
		mov	dx,03c4h
		mov	ax,0604h
		out	dx,ax
		mov	dx,03d4h
		mov	al,014h
		out	dx,al
		inc	dx
		in	al,dx
		and al,0bfh
		out	dx,al
		mov	dx,03d4h
		mov	al,17h
		out	dx,al
		inc	dx
		in	al,dx
		or	al,040h
		out	dx,al
		mov	al,5
		mov	dx,03ceh
		out	dx,al
		inc	dx
		in	al,dx
		and al,0ech
		out	dx,al
		mov	dx,03ceh
		mov	al,6
		out	dx,al
		inc	dx
		in	al,dx
		and al,0fdh
		out	dx,al
		mov	dx,03c4h
		mov	ax,0f02h
		out	dx,ax
		mov ax,0A000h  	// Now clear all active memory
		mov es,ax
		xor di,di
		xor ax,ax
		mov cx,8000h    // all 256k memory
		cld
		rep stosw
		pop di
		pop es
	}
}


// Function to set the plane write enable register in Graphics controller
void WritePlaneEnable(char En)
{
	asm {
		mov	dx,03ceh	// select plane enable register
		mov	al,1
		out	dx,al
		inc	dx
		mov	al,En
		out	dx,al
	}
}

// Function to set read plane
void ReadPlaneEnable(char En)
{
	asm {
		mov	dx,03ceh
		mov	al,4
		out	dx,al
		inc	dx
		mov	al,En
		out	dx,al
	}
}

// Function to set the write mode
void WriteMode(char m)
{
	asm {
		mov	dx,03ceh
		mov	al,5		// select memory mode register
		out	dx,al
		inc	dx
		in	al,dx
		and	al,0fch
		or	al,m
		out	dx,al		// store to write mode register
	}
}

// Routine to set VGA hardware to point to page specified in ModeX
void xShowPage(void)
{
	xSetPage(PageOffsets[NonDisplayedPage]);
	DisplayedPage=DisplayedPage ^ 1;
	NonDisplayedPage=NonDisplayedPage ^ 1;
}

// dummy function to swap value of pages.  used to properley erase moving objects
void xSwapPage(void)
{
	DisplayedPage=DisplayedPage ^ 1;
	NonDisplayedPage=NonDisplayedPage ^ 1;
}
// Function to switch to VGA text mode 3.
void TextMode(void)
{
	asm {
		xor	ah,ah
		mov	al,03h		// Mode 03
		int	10h			// Video interupt
	}
}

// Function to load entire VGA 256 colour palette
void LoadPalette(PalData *Palette)
{
	int i;

	for(i=0;i<256;i++) {
		outportb(DacWrite,i);
		outportb(DacData,Palette->Red[i]);
		outportb(DacData,Palette->Green[i]);
		outportb(DacData,Palette->Blue[i]);
	}
	return;
}

// Function to display an image given a far pointer to 64000 bytes of
// compressed image data.  Palette needs to be transferred seperately.
void DisplayImage(char far *ImageData, char far *buffer)
{
	unsigned int NumBytes = 0;	// Byte Counter for screen
	unsigned int InPointer = 0;
	char far *ScrPointer;
	unsigned char c;
	unsigned char runlen = 0;

	if(buffer==0) {
		buffer=(char far *)0xa0000000L;	// Setup screen pointer
	}
	ScrPointer=buffer;
	do {
		c = ImageData[InPointer++];	// get first character
		if((c & 0xc0) == 0xc0) {
			runlen = (c & 0x3f);	// get runbytes
			c = ImageData[InPointer++]; // get repeat character
			while(runlen--) ScrPointer[NumBytes++]=c;
		} else {
			ScrPointer[NumBytes++]=c;
		}
	} while(NumBytes<64000);

	return;
}

// Function to displau a Compressed PCX file in a selectable page on a
// ModeX screen
void DisplayImageX(char far *ImageData, int page)
{
	unsigned int NumBytes = 0;	// Byte Counter for screen
	unsigned int InPointer = 0;
	char far *ScrPointer;
	unsigned char c;
	unsigned char runlen = 0;

	ScrPointer=(char far *)farmalloc(64000L);
	if(ScrPointer==NULL) {	// no memory for tempoaray image
		cout << "Not enough memory for image load" << endl;
		return;
	}
	do {
		c = ImageData[InPointer++];	// get first character
		if((c & 0xc0) == 0xc0) {
			runlen = (c & 0x3f);	// get runbytes
			c = ImageData[InPointer++]; // get repeat character
			while(runlen--) ScrPointer[NumBytes++]=c;
		} else {
			ScrPointer[NumBytes++]=c;
		}
	} while(NumBytes<64000);
	xPutImage(ScrPointer, PageOffsets[page]);	// put to MODEX screen
	farfree(ScrPointer);	// deallocate memory from temporary buffer
	return;					// and return
}

// Function to read a PCX file and store in a PCX structure
// Takes a path name and returns pointer to PCX structure or 0 on error
PCX *ReadImage(char *fname)
{
	FILE *fptr;
	PCX *PCXInfo = new PCX;
	unsigned char far *paltemp = new char[769];	// allocate memory for palette
	long ReadSize;
	int i,t;

	PCXInfo->ImageData=0L;
	if((fptr=fopen(fname, "rb")) == 0) {delete PCXInfo;return NULL;}
	if(fseek(fptr, -768L, SEEK_END) !=0 ) {
		fclose(fptr);
		delete PCXInfo;
		return NULL;	// Return on errors;
	}
	if(fread(paltemp, 1, 768, fptr) != 768) {
		fclose(fptr);
		delete PCXInfo;
		return NULL;
	}
	if(fseek(fptr, 128, SEEK_SET) !=0) {
		fclose(fptr);
		delete PCXInfo;
		return NULL;
	}
	for(t=0;t<256;t++) {
		PCXInfo->Palette->Red[t]=*(paltemp+(t*3))>>2;
		PCXInfo->Palette->Green[t]=*(paltemp+(t*3)+1)>>2;
		PCXInfo->Palette->Blue[t]=*(paltemp+(t*3)+2)>>2;
	}
	ReadSize=filelength(fileno(fptr))-(768+128);	// Get image size
	if((PCXInfo->ImageData=new char[ReadSize]) == NULL) {	// allocate memory
		fclose(fptr);
		delete PCXInfo;
		return NULL;
	}
	if(fread(PCXInfo->ImageData, 1, ReadSize, fptr) != ReadSize) {
		fclose(fptr);
		free(PCXInfo->ImageData);
		PCXInfo->ImageData=0L;
		delete PCXInfo;
		return NULL;
	}	// Read image data
	fclose(fptr);	// close file
	delete paltemp;	// delete temporary palette
	return PCXInfo;	// Return new structure
}

// Function to fade from black to current palette colour
void Fade2Palette(PalData *Palin)
{
	int i, n;
	PalData *BPalette = new PalData;	// Black palette
	PalData *Palette = new PalData;	  	// Colour palette

	for(i=0;i<256; i++) {
		BPalette->Red[i]=0;
		BPalette->Green[i]=0;
		BPalette->Blue[i]=0;
	}
	for(i=0;i<256;i++) {
		Palette->Red[i]=Palin->Red[i]&0x3f;	// Strip of lower 6 bits
		Palette->Green[i]=Palin->Green[i]&0x3f;
		Palette->Blue[i]=Palin->Blue[i]&0x3f;
	}
	for(n=0;n<64;n++) {		// 64 step fade
		WaitRetrace();		// Wait for vertical retrace
		for(i=0;i<256;i++) {	// 256 colour palette
			asm cli;
			outportb(DacWrite, i);	// Select palette register
			outportb(DacData, BPalette->Red[i]);	// now send colour
			if((64-n) <= Palette->Red[i]) BPalette->Red[i]++;
			outportb(DacData, BPalette->Green[i]);
			if((64-n) <= Palette->Green[i]) BPalette->Green[i]++;
			outportb(DacData, BPalette->Blue[i]);
			if((64-n) <= Palette->Blue[i]) BPalette->Blue[i]++;
			asm sti;
		}
	}
	return;
}

// Function to clear colour palette to zero
void ClearPalette(void)
{
	int i;

	for(i=0;i<256;i++) {
		outportb(DacWrite, i);	// Setup palette register
		outportb(DacData, 0);	// Black
		outportb(DacData, 0);	// Black
		outportb(DacData, 0);	// Black
	}
	return;
}

// Function to fade from current palette colours to black
void Fade2Black(PalData *Palin)
{
	int i, n;
	PalData *Palette = new PalData;			// Colour palette

	for(i=0;i<256;i++) {
		Palette->Red[i]=Palin->Red[i]&0x3f;	// Strip of lower 6 bits
		Palette->Green[i]=Palin->Green[i]&0x3f;
		Palette->Blue[i]=Palin->Blue[i]&0x3f;
	}
	for(n=0;n<64;n++) {		// 64 step fade
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