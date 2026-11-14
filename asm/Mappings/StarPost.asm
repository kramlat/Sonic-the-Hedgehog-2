; ---------------------------------------------------------------------------
; The star post (object $79): Star_Post_Mappings of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl
; Frames: 0 the post and its ball, 1 without the ball, 2 the ball, 3 the ball (the other palette), 4 the post with the other palette ball
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
		dc.w .f3-.int
		dc.w .f4-.int
.f0:		dc.w 4		; loc_1B2BE
		dc.w $E801, $2004, $2002, $FFFC
		dc.w $F803, $2006, $2003, $FFF8
		dc.w $F803, $2806, $2803, $0000
		dc.w $D805, $0000, $0000, $FFF8
.f1:		dc.w 3		; loc_1B2E0
		dc.w $E801, $2004, $2002, $FFFC
		dc.w $F803, $2006, $2003, $FFF8
		dc.w $F803, $2806, $2803, $0000
.f2:		dc.w 1		; loc_1B2FA
		dc.w $F805, $0000, $0000, $FFF8
.f3:		dc.w 1		; loc_1B304
		dc.w $F805, $2000, $2000, $FFF8
.f4:		dc.w 4		; loc_1B30E
		dc.w $E801, $2004, $2002, $FFFC
		dc.w $F803, $2006, $2003, $FFF8
		dc.w $F803, $2806, $2803, $0000
		dc.w $D805, $2000, $2000, $FFF8
