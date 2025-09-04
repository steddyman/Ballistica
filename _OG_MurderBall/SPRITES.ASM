IDEAL
P386
MODEL FARSTACK LARGE

;Sourcecode for sprite blit routines

	DATASEG
SCRDEPTH equ 200  ; set number of screen lines
SCRWIDTH equ 320  ; set screen width
DEFH equ 9
BALLID	equ 200
DEFHEIGHT equ 9
DEFWIDTH equ 16
BALLWIDTH equ 3
BALLHEIGHT equ 3
SCRSEG   equ 0a000h ; set screen segment address
EXTRN _maskbuffer : DWORD
EXTRN _ballbuffer : DWORD

	CODESEG
	ORG 0

LeftMask	db  0fh, 0eh, 0ch, 08h
RightMask	db	01h, 03h, 07h, 0fh
BothMasks	db	01h, 02h, 04h, 08h
lineoffsets dw SCRDEPTH dup(?)

;void fillOffsets(void)
;	Routine to fill in the line offsets table above with the
;	offsets for the start of each line
PUBLIC C fillOffsets
PROC C fillOffsets

	push    si di es
	cld
	mov     di,OFFSET lineoffsets   ;get pointer to line offsets
	mov     ax,SEG lineoffsets  ;get segment to above
	mov     es,ax
	mov     cx,SCRDEPTH         ;CX = line number
	mov     ax,0                ;AX = start offset (0)
	 @@nextline:
	stosw                       ;store current offset
	add     ax,SCRWIDTH         ;add offset per screen line
	loop    @@nextline          ;continue for length of screen
	pop     es di si
	retf
ENDP

;void xPutImage(char far *source, int off)
;	Rotuine to transfer the image file stored in source to the
;	screen page pointed to by offset.  Size of transfer is 320x200
PUBLIC C xPutImage
PROC C xPutImage
ARG source:DATAPTR, off:WORD

	push	si di es ds
	cld
	mov		ax,0a000h				;Load pointer to screen segment
	mov		es,ax					;and store to destination segment
	mov		di,[off]				;get offset from passed value
	lds		si,[source]				;Get source pointer from passed value
	mov		bx,(SCRWIDTH*SCRDEPTH)/4	;and size of destination transfer
	mov		dx,03c4h				;set DX to point to Map Mask register
		@@nextline:		;start of line routine
	mov		cl,1
	mov		ah,cl
		@@nextcol:		;start of current column routine
	mov		al,2
	out		dx,ax		;select the current plane for writing
	lodsb				;get source byte in al
	mov		[es:di],al	;and store to destination
	shl		cl,1		;increment plane number
	mov		ah,cl
	and		ah,0fh		;mask of the plane counter
	jnz		@@nextcol		;not back to zero so next byte
	inc		di			;increment destination counter
	dec		bx			;and decrement bytes left
	jnz		@@nextline	;and back to next line if some left
	pop		ds es di si
	leave
	retf
ENDP

;void blitSprite(int x, int y, int height, int width,
;				 char far *sprite)
;	Routine to draw sprite directly onto the screen. Can be any size
PUBLIC C blitSprite
PROC C blitSprite
ARG x:WORD, y:WORD, h:WORD, w:WORD, sprite:DATAPTR

	push    si di es ds
	cld
	lds     si,[sprite]             ;DS:SI = sprite data
	mov		ax,ds
	or		ax,si
	jz		@@endsprite				;if no sprite data, end
	mov     ax,SCRSEG
	mov     es,ax                   ;set ES to screen segment
	mov     bx,[y]                  ;get screen start line. BX = current line
	mov     dx,[h]                  ;get height in DX
	  @@nextdraw:                   ;draw line start point
	shl     bx,1
	mov     di,[cs:lineoffsets+bx]  ;get offset to screen line
	shr     bx,1
	add     di,[x]                  ;add x offset
	mov     cx,[w]                  ;get number of bytes to move
	shr     cx,1                    ;and convert to words
	rep     movsw                   ;store sprite
	jnc     @@skipdraw              ;and check if a byte left
	movsb                           ;store last byte if necessary
	  @@skipdraw:
	inc     bx                      ;increment line number
	dec     dx                      ;decrement line counter
	jnz     @@nextdraw              ;and goto next line if not finished
	  @@endsprite:
	pop     ds es di si
	leave
	retf                            ;pop registers and return
ENDP

;void eraseSprite(int x, int y, int height, int width,
;				 char far *background)
;	Routine to erase sprite from the screen. Can be any size
PUBLIC C eraseSprite
PROC C eraseSprite
ARG x:WORD, y:WORD, h:WORD, w:WORD, background:DATAPTR

	push    si di es ds
	cld
	lds     si,[background]             ;DS:SI = background data
	mov		ax,ds
	or		ax,si
	jz		@@endsprite				;if no sprite data, end
	mov     ax,SCRSEG
	mov     es,ax                   ;set ES to screen segment
	mov     bx,[y]                  ;get screen start line. BX = current line
	mov     dx,[h]                  ;get height in DX
	  @@nextdraw:                   ;draw line start point
	lds     si,[background]             ;DS:SI = background data
	shl     bx,1
	mov     di,[cs:lineoffsets+bx]  ;get offset to screen line
	shr     bx,1
	add     di,[x]                  ;add x offset
	add		si,di					;also same offset on source
	mov     cx,[w]                  ;get number of bytes to move
	shr     cx,1                    ;and convert to words
	rep     movsw                   ;store sprite
	jnc     @@skipdraw              ;and check if a byte left
	movsb                           ;store last byte if necessary
	  @@skipdraw:
	inc     bx                      ;increment line number
	dec     dx                      ;decrement line counter
	jnz     @@nextdraw              ;and goto next line if not finished
	  @@endsprite:
	pop     ds es di si
	leave
	retf                            ;pop registers and return
ENDP

;void blitBrick(int x, int y, char far *sprite)
;	Extremely fast routine specialising in drawing the brick on the screen
PUBLIC C blitBrick
PROC C blitBrick
ARG x:WORD, y:WORD, sprite:DATAPTR

	push	ds es esi edi
	mov		ax,0a000h
	mov		es,ax
	xor		edi,edi			;Point ED:DI to screen segment
	lds		si,[sprite]		;Point DS:SI to sprite data
	mov		bx,[y]
	shl		bx,1
	add		di,[cs:lineoffsets+bx]
	add		di,[x]			;Calculate address of first pixel into DI
	mov		cx,DEFHEIGHT
		@@drawloop:
	movsd
	movsd
	movsd
	movsd					;16 pixels wide
	add		di,SCRWIDTH-DEFWIDTH	;point to next pixel below one in line
	loop	@@drawloop		;repeat for number of pixels down (9)
	pop		edi esi es ds
	leave
	retf
ENDP

;void blitBrickBack(int x, int y, char far *sprite, char far *backptr)
;	Extremely fast routine specialising in drawing the brick on the screen
;	but this time into the offscreen background buffer
PUBLIC C blitBrickBack
PROC C blitBrickBack
ARG x:WORD, y:WORD, sprite:DATAPTR, backptr:DATAPTR

	push	ds es esi edi
	xor		edi,edi
	xor		esi,esi
	lds		si,[sprite]		;Point DS:SI to sprite data
	les		di,[backptr]	;Point ES:DI to background data
	mov		bx,[y]
	shl		bx,1
	add		di,[cs:lineoffsets+bx]
	add		di,[x]			;Calculate address of first pixel into DI
	mov		cx,DEFHEIGHT
		@@drawloop:
	movsd
	movsd
	movsd
	movsd					;16 pixels wide
	add		di,SCRWIDTH-DEFWIDTH	;point to next pixel below one in line
	loop	@@drawloop		;repeat for number of pixels down (9)
	pop		edi esi es ds
	leave
	retf
ENDP

;void eraseBrick(int x, int y, char far *background)
;	Extremely fast routine specialising in erasing the brick on the screen
;	background is a pointer to start of offscreen buffer
PUBLIC C eraseBrick
PROC C eraseBrick
ARG x:WORD, y:WORD, background:DATAPTR

	push	ds es esi edi
	mov		ax,0a000h
	mov		es,ax
	xor		edi,edi			;Point ED:DI to screen segment
	lds		si,[background]		;Point DS:SI to background
	mov		bx,[y]
	shl		bx,1
	add		di,[cs:lineoffsets+bx]
	add		di,[x]			;Calculate address of first pixel into DI
	add		si,di			;also onto source pointer
	mov		cx,DEFHEIGHT
		@@drawloop:
	movsd
	movsd
	movsd
	movsd					;16 pixels wide
	add		si,SCRWIDTH-DEFWIDTH
	add		di,SCRWIDTH-DEFWIDTH	;point to next pixel below one in line
	loop	@@drawloop		;repeat for number of pixels down (9)
	pop		edi esi es ds
	leave
	retf
ENDP

;void blitBall(int x, int y, char far *sprite)
;	Extremely fast routine specialising in drawing the ball on the screen
PUBLIC C blitBall
PROC C blitBall
ARG x:WORD, y:WORD, sprite:DATAPTR

	push	ds es esi edi
	mov		ax,0a000h
	mov		es,ax
	xor		edi,edi			;Point ED:DI to screen segment
	lds		si,[sprite]		;Point DS:SI to sprite data
	mov		bx,[y]
	shl		bx,1
	add		di,[cs:lineoffsets+bx]
	add		di,[x]			;Calculate address of first pixel into DI
	mov		cx,BALLHEIGHT
		@@drawloop:
	movsw
	movsb		;3 pixels wide
	add		di,SCRWIDTH-BALLWIDTH	;point to next pixel below one in line
	loop	@@drawloop		;repeat for number of pixels down (9)
	pop		edi esi es ds
	leave
	retf
ENDP

;void eraseBall(int x, int y, char far *background)
;	Extremely fast routine specialising in erasing the ball on the screen
;	background is a pointer to start of offscreen buffer
PUBLIC C eraseBall
PROC C eraseBall
ARG x:WORD, y:WORD, background:DATAPTR

	push	ds es esi edi
	mov		ax,0a000h
	mov		es,ax
	xor		edi,edi			;Point ED:DI to screen segment
	lds		si,[background]		;Point DS:SI to background
	mov		bx,[y]
	shl		bx,1
	add		di,[cs:lineoffsets+bx]
	add		di,[x]			;Calculate address of first pixel into DI
	add		si,di			;also onto source pointer
	mov		cx,BALLHEIGHT
		@@drawloop:
	movsw
	movsb			; 3 pixel width
	add		si,SCRWIDTH-BALLWIDTH
	add		di,SCRWIDTH-BALLWIDTH	;point to next pixel below one in line
	loop	@@drawloop		;repeat for number of pixels down (9)
	pop		edi esi es ds
	leave
	retf
ENDP


;void xblitSprite(int destx, int desty, int h, int w,
;				  int sourcex, int sourcey, int frompage, int topage)
;	Routine to draw a sprite in ModeX from one area of the display
;	memory to another area of the display memory.
;   	destx, desty = location to draw sprite
;		sourcex, sourcey = location of sprite to copy from display memory
;		h, w = physical size of sprite in pixels
;	   frompage, topage = source and destination offsets
PUBLIC C xblitSprite
PROC C xblitSprite
LOCAL linesleft:WORD
ARG destx:WORD, desty:WORD, h:WORD, w:WORD, sourcex:WORD, sourcey:WORD, frompage:WORD, topage:WORD

	push si di es ds			;save registers to be used
	cld
	mov		ax,[destx]			;get destination column
	and		ax,3				;mask off bottom two bits
	mov		si,ax
	mov		bh,[cs:LeftMask+si]	;and use to get left mask value
	mov		ax,[destx]			;get destination again
	add		ax,[w]				;find the last column
	dec		ax					;subtract 1
	and		ax,3				;again mask off bottom two bits
	mov		si,ax
	mov		bl,[cs:RightMask+si]	;and use as index for right screen mask
	push	bx					;save mask for use later
	mov		ax,0a000h			;load ax with screen segment address
	mov		ds,ax
	mov		es,ax				;and transfer to source and destination segment
	mov		bx,SCRWIDTH/4		;put number of bytes a line in BX
	mov		ax,[desty]			;and destination row into AX
	mul		bx					;and get offset to start of line in AX
	mov		bx,[destx]			;load the destination column into BX
	shr		bx,2				;divide it by 4
	add		ax,bx				;and add to AX. AX=offset to destination byte
	add		ax,[topage]			;Finally add offset to destination page
	mov		di,ax				;and move it into destination offset register
	mov		ax,[sourcey]		;now get start row for source of transfer
	mov		bx,SCRWIDTH/4		;length of line into BX again
	mul		bx					;and get offset to start of line in AX
	mov		bx,[sourcex]		;get the start column of source into BX
	shr		bx,2				;divide it by 4
	add		ax,bx				;and add to AX. AX=offset to source byte
	add		ax,[frompage]		;Finally add offset to source page
	mov		si,ax				;and move it into the source offset register
		;DS:SI now point to first byte from transfer source and
		;ES:DI points to first byte of destination address
	pop		bx					;retreive left and right masks from stack
	mov		dx,03c4h			;Setup sequencer to point to map mask register
	mov		al,2				;which is index 2
	out		dx,al
	inc		dx					;DX now points to MAPMASK port
	mov		ax,[destx]			;Get destination column into AX
	mov		cx,ax				;put into CX use below
	shr		ax,2				;divide by 4 to get location in byte form
	add		cx,[w]				;CX now equlat destination column+1
	dec		cx					;correct to last column in pixels
	shr		cx,2				;and convert to byte form
	sub		cx,ax				;CX now equals number of bytes across line
	jnz		@@startline			;if more than one byte, goto transfer routine
	and		bh,bl				;else combine the left and right masks
		;BX now equals the Left and Right Map Mask values for both edges
		;CX equals the number of bytes to move per line
		@@startline:
	push	cx					;save line length, will be needed later
	push	si					;save start of line address for source
	push	di					;and also for destination
	mov		al,bh
	out		dx,al				;output left mask value
	movsb						;and transfer first byte
	dec		cx					;decrement line byte counter
	js		@@linedone			;and if only byte, go onto next line setup
	jz		@@doright			;or only two bytes, goto right mask value
	mov		al,0fh				;enable all planes value
	out		dx,al				;and send to MAPMASK port
	rep		movsb				;transfer CX remaining bytes on line
		@@doright:
	mov		al,bl
	out		dx,al				;out mask for right hand edge of sprite
	movsb						;and transfer final byte
		@@linedone:
	pop		di					;retrieve start of line offset for destination
	add		di,SCRWIDTH/4		;add the length of a line to it
	pop		si					;now do the same for the source offset
	add		si,SCRWIDTH/4
	pop		cx					;retrieve byte line count for use again
	dec		[h]					;Decrement the passed height counter
	jnz		@@startline			;and if any line left, go back and process
	pop     ds es di si			;restore saved registers
	leave                   	;restore stack to previous state
	retf						;and return
ENDP

;void xeraseSprite(int x, int y, int h, int w, int off, char far *background)
;	Routine will draw background data pointed to by the background
;		pointer onto the background mode x screen with page offset
PUBLIC C xeraseSprite
PROC C xeraseSprite
ARG x:WORD, y:WORD, h:WORD, w:WORD, off:WORD, background:DATAPTR

	push	esi di ds es	;note ESI is going to be used (32bits)
	mov		di,[off]
	mov		ax,SCRSEG		;ES:DI pointer to a000:offset
	mov		es,ax
	xor		esi,esi
	lds		si,[background]	;load pointer to start of page
	mov		bx,[y]
	mov		eax,SCRWIDTH	;screen source is 320 bytes wide
	mul		bx				;only multiplies with AX (ok)
	add		ax,[x]			;ax = offset to x and y in background
	add		esi,eax			;add onto source offset
		;Now must add on offsets of x and y to DI
	mov		bx,[y]
	mov		ax,SCRWIDTH/4	;screen dest (mode x) only 80 bytes wide
	mul		bx
	mov		bx,[x]
	shr		bx,2
	add		ax,bx
	add		di,ax			;now DI is pointing at correct byte on screen
	mov		dx,03c4h		;choose the MAP MASK register
	mov		al,2			;which is register 2
	out		dx,al
	inc		dx				;and point DX to correct port address
	push	esi di			;save pointer to start of source/dest lines
		@@startline:		;start of line routine
	mov		ax,1
	mov		cx,[x]
	and		cl,3			;work out which plane first byte is in
	shl		al,cl			;find bit mask for current x
	mov		bl,al			;and mov into BL
	mov		cx,[w]			;width into CX
	shr		cx,2			;divide by 4 (mode x)
	je		@@rightside		;if less than 4 bytes wide (a ball)
	push	cx				;save for use on next plane
	mov		al,bl
	out		dx,al			;output first byte
		@@loop1:			;loop for first plane draw routine
	lodsd					;32 bit read, si=si+4
	stosb					;only bottom 8 bits required, di=di+1
	loop	@@loop1			;repeat for width of sprite
	pop		cx di esi		;retreive push registers (reset start of line)
	shl		bl,1			;select the next mask plane
	and		bl,0fh
	jnz		@@skip1
	mov		bl,1			;mov onto next byte, reset mask and inc dest
	inc		di
		@@skip1:
	push	esi di cx		;and resave for below
	mov		al,bl
	out		dx,al			;output to map mask register
		@@loop2:
	lodsb
	shr		eax,8			;get the bits required into al
	stosb
	loop	@@loop2
	pop		cx di esi		;retreive push registers (reset start of line)
	shl		bl,1			;select the next mask plane
	and		bl,0fh
	jnz		@@skip2
	mov		bl,1			;mov onto next byte, reset mask and inc dest
	inc		di
		@@skip2:
	push	esi di cx		;and resave for below
	mov		al,bl
	out		dx,al			;output to map mask register
		@@loop3:
	lodsb
	shr		eax,8			;get the bits required into al
	stosb
	loop	@@loop3
	pop		cx di esi		;retreive push registers (reset start of line)
	shl		bl,1			;select the next mask plane
	and		bl,0fh
	jnz		@@skip3
	mov		bl,1			;mov onto next byte, reset mask and inc dest
	inc		di
		@@skip3:
	push	esi di cx		;and resave for below
	mov		al,bl
	out		dx,al			;output to map mask register
		@@loop4:
	lodsb
	shr		eax,8			;get the bits required into al
	stosb
	loop	@@loop4
	pop		cx				;retreive width and discard it
		@@rightside:
	mov		cx,[w]			;get width again
	and		cx,3			;get number of pixels remaining to draw
	jz		@@doneline		;and if none, go onto next line
	mov		bl,cl			;get start mask into bl
			@@rightloop:
	mov		al,bl
	out		dx,al
	movsb					;SI, DI already pointing at the end of last line
	shl		bl,1			;shift up the plane
	and		bl,0fh
	jz		@@skip4			;now jump if zero, needs reloading
	dec		di				;dec dest pointer as occupies same location in mem
	loop	@@rightloop		;back up if more to do
	jmp		@@doneline		;else done
		@@skip4:
	mov		bl,1
	loop	@@rightloop
		@@doneline:
	pop		di esi			;retreive pointers to start of line
	dec		[h]				;decrement passed height counter
	je		@@done
	add		di,SCRWIDTH/4	;increment screen counter by +1 in y direction
	add		si,SCRWIDTH		;position to next line in background (320 bytes)
	push	esi di
	jmp		@@startline		;and continue with next line
		@@done:				;all finished
	pop		es ds di esi	;retreive registers
	leave					;restore stack
	retf					;and return
ENDP


;xSetPage(int offset)
;Routine to switch the currenty displayed VGA page
PUBLIC C xSetPage
PROC C xSetPage
ARG poffset:WORD

		mov	bl,0ch				; Load bl with index to Start Address Low
		mov	dx,[poffset]
		mov	bh,dh
		mov	cl,0dh				; Load cl with index to Start Address High
		mov	ch,dl
		mov	dx,03dah			; DX equals input status
			@@WaitDisplayEnable:
		in	al,dx
		test	al,01h			; check DE flag
		jnz	@@WaitDisplayEnable
		mov	dx,03d4h			; DX equals CRTC controller
		mov	ax,bx
		out	dx,ax
		mov	ax,cx
		out	dx,ax
		mov	dx,03dah			; reload input status register
			@@WaitDisplayRetrace:
		in	al,dx
		test	al,08h
		jz	@@WaitDisplayRetrace
		leave
		retf
ENDP

;void xcopyPage(int srcoff, int destoff)
;	Routine to move one full page to another using fast mov instructions
;		4 pixels at a time
PUBLIC C xcopyPage
PROC C xcopyPage
ARG srcoff:WORD, destoff:WORD

	push	si di ds es
	mov		ax,SCRSEG		;get screen segment address
	mov		ds,ax
	mov		es,ax			;and move into both DS and ES
	mov		si,[srcoff]		;move source offset into ESI
	mov		di,[destoff]	;and destination into edi
	mov		cx,SCRWIDTH*SCRDEPTH
	shr		cx,2			;divide by 4 (movsd 4 bytes*4 planes)
	mov		dx,03c4h		;setup map mask register
	mov		ax,0f02h		;byte to select register 2 and enable all planes
	out		dx,ax
	rep		movsb			;do transfer
	pop		es ds di si		;restore registers
	leave
	retf					;and return
ENDP

;void blitMask(int x, int y, int height, int width,
;				 int id, char far *buffer)
;	Routine to draw object id into offscreen buffer.
;	if id==0 then erased from buffer
PUBLIC C blitMask
PROC C blitMask
ARG x:WORD, y:WORD, h:WORD, w:WORD, id:WORD, buffer:DATAPTR

	push    di es
	cld
	les     di,[buffer]             ;ES:DI = address of mask buffer
	mov		ax,es
	or		ax,di
	jz		@@endmask				;if no destination then end
	mov     bx,[y]                  ;get screen start line. BX = current line
	mov     dx,[h]                  ;get height in DX
	  @@nextdrawmask:               ;draw line start point
	push	di
	shl     bx,1
	add     di,[cs:lineoffsets+bx]  ;get offset to screen line
	shr     bx,1
	add     di,[x]                  ;add x offset
	mov     cx,[w]                  ;get number of bytes to move
	mov		ax,[id]					;get id to store
	mov		ah,al
	shr		cx,1
	rep     stosw                   ;and move into buffer memory
	jnc		@@skipbyte
	stosb
	  @@skipbyte:
	inc     bx                      ;increment line number
	dec     dx                      ;decrement line counter
	pop		di
	jnz     @@nextdrawmask          ;and goto next line if not finished
	  @@endmask:
	pop     es di
	leave
	retf                            ;pop registers and return
ENDP

;int getMaskPixel(int x, int y)
;function to read the value of a pixel from the virtual screen array
;pointed to by maskbuffer
PUBLIC C getMaskPixel
PROC C getMaskPixel
ARG x:WORD, y:WORD

	push    si ds
	cld
	lds     si,[_maskbuffer]             ;DS:SI = address of mask buffer
	mov     bx,[y]                  ;get screen start line. BX = current line
	shl     bx,1
	add     si,[cs:lineoffsets+bx]  ;get offset to screen line
	add     si,[x]                  ;add x offset
	xor		ax,ax
	lodsb							; get byte from mask
	pop     ds si
	leave
	retf                            ;pop registers and return
ENDP

;int setMaskPixel(int x, int y, int id)
;function to write the value of a pixel into the ballbuffer
PUBLIC C setMaskPixel
PROC C setMaskPixel
ARG x:WORD, y:WORD, id:WORD

	push    di es
	cld
	les     di,[_ballbuffer]             ;DS:SI = address of mask buffer
	mov     ax,[y]                  ;get screen start line. BX = current line
	mov     bx,320
	mul		bx
	add     ax,[x]                  ;add x offset
	add		di,ax
	xor		ax,ax
	mov		ax,[id]
	stosb							; set byte in mask
	pop     es di
	leave
	retf                            ;pop registers and return
ENDP

;int testMaskLine(int x, int y)
;function to check a vertical line in maskbuffer screen array for a value
;signifying the ball sprite
PUBLIC C testMaskLine
PROC C testMaskLine
ARG x:WORD, y:WORD

	push	si ds
	cld
	lds		si,[_ballbuffer]
	mov		ax,[y]
	mov		bx,320
	mul		bx
	add		ax,[x]
	add		si,ax
	mov		cx,DEFH
	  @@readloop:					; maskbuffer
	mov		al,[ds:si]
	cmp		al,BALLID
	je		@@donetrue
	add		si,bx
	loop	@@readloop
	jmp		@@donefalse
	  @@donetrue:
	mov		ax,1
	jmp		@@done
	  @@donefalse:
	mov		ax,0
	  @@done:
	pop		ds si
	leave
	retf
ENDP
ENDS
END
