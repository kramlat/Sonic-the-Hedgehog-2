; ---------------------------------------------------------------------------
; The egg prison (object $3E): Egg_Prison_Mappings of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl
; Frames: 0-3 the capsule closed to open, 4 the button, 5 the lid
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
		dc.w .f3-.int
		dc.w .f4-.int
		dc.w .f5-.int
.f0:		dc.w 7		; loc_3B286
		dc.w $E00D, $0000, $0000, $FFE0
		dc.w $E00D, $0800, $0800, $0000
		dc.w $F00A, $0008, $0004, $FFE0
		dc.w $F006, $0011, $0008, $FFF8
		dc.w $F00A, $0808, $0804, $0008
		dc.w $080E, $0017, $000B, $FFE0
		dc.w $080E, $0817, $080B, $0000
.f1:		dc.w 7		; loc_3B2C0
		dc.w $E00D, $0000, $0000, $FFE0
		dc.w $E00D, $0800, $0800, $0000
		dc.w $080E, $0017, $000B, $FFE0
		dc.w $080E, $0817, $080B, $0000
		dc.w $E806, $0011, $0008, $FFF8
		dc.w $F80A, $0008, $0004, $FFE0
		dc.w $F80A, $0808, $0804, $0008
.f2:		dc.w 7		; loc_3B302
		dc.w $E00D, $0000, $0000, $FFE0
		dc.w $E00D, $0800, $0800, $0000
		dc.w $080E, $0017, $000B, $FFE0
		dc.w $080E, $0817, $080B, $0000
		dc.w $E006, $0011, $0008, $FFF8
		dc.w $000A, $0008, $0004, $FFE0
		dc.w $000A, $0808, $0804, $0008
.f3:		dc.w 5		; loc_3B344
		dc.w $E00D, $0000, $0000, $FFE0
		dc.w $E00D, $0800, $0800, $0000
		dc.w $F006, $0023, $0011, $FFF8
		dc.w $080E, $0017, $000B, $FFE0
		dc.w $080E, $0817, $080B, $0000
.f4:		dc.w 2		; loc_3B36E
		dc.w $F805, $0029, $0014, $FFF0
		dc.w $F805, $0829, $0814, $0000
.f5:		dc.w 1		; loc_3B380
		dc.w $F805, $002D, $0016, $FFF8
