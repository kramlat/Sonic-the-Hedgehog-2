; ---------------------------------------------------------------------------
; The versus results banner (object $21): _2P_Versus_Results_Mappings of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl
; Frames: 0 1P WINS, 1 2P WINS, 2 TIED
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
.f0:		dc.w 3		; _1P_Wins_Mappings
		dc.w $F80D, $0000, $0000, $0000
		dc.w $F80D, $000C, $0006, $0028
		dc.w $F809, $0014, $000A, $0048
.f1:		dc.w 4		; _2P_Wins_Mappings
		dc.w $F805, $0008, $0004, $0000
		dc.w $F805, $0004, $0002, $0010
		dc.w $F80D, $000C, $0006, $0028
		dc.w $F809, $0014, $000A, $0048
.f2:		dc.w 2		; Tied_Mappings
		dc.w $F80D, $001A, $000D, $0018
		dc.w $F805, $0022, $0011, $0038
