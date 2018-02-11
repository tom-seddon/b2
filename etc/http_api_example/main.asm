	*=$1000

	lda #22
	jsr $ffee
	lda #4
	jsr $ffee
	;; 	sei
fill:
_loop:	
	lda $fe45
_write
	sta $5800
	inc _write+1
	bne _loop
	inc _write+2
	bpl _loop

scroll:
	lda $5800
	pha
	
	ldx #0
_loop:	
	.for p = $5800, p < $8000, p = p + $100
	lda p+1,x
	sta p+0,x
	.next

	inx

	beq _done
	jmp _loop
_done:	
	pla
	sta $7fff

	jmp scroll
	
