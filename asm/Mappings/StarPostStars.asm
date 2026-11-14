; ---------------------------------------------------------------------------
; The bonus stars of the star post: Star_Post_Stars_Mappings of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl
; Frames: 0 a big star, 1 and 2 small ones
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
.f0:		dc.w 1		; loc_1B336
		dc.w $F805, $000A, $0005, $FFF8
.f1:		dc.w 1		; loc_1B340
		dc.w $FC00, $000E, $0007, $FFFC
.f2:		dc.w 1		; loc_1B34A
		dc.w $FC00, $000F, $0007, $FFFC
