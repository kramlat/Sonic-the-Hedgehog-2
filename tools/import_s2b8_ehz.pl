#!/usr/bin/perl
# Brings Sonic 2 Beta 8's Emerald Hill objects and rings (the later, almost complete placements) into res/S2Objects/EHZ1-2 and res/S2Rings/EHZ1-2. S2B8 gives the disassembly's folder, default
# ~/Projects/reference/s2disasm-beta8 (its level/objects and level/rings). Its foreground layouts are byte for byte ours (checked 2026-11-27), so the layouts, and the backgrounds that have been optimized, are not touched.
# The object layout is the engine's format already (six bytes an object, the end mark FFFF); the ring layout is groups (a line of up to 8 rings in an entry) and the ring handler (Rings.h) wants one entry for each
# ring in order by X, with a 4-byte end mark (as tools/import_s2b4_ehz.pl's sort_rings).
# Run it in the Sonic 2 folder: import_s2b8_ehz.pl
use strict;
use warnings;

my $src = ($ENV{S2B8} // "$ENV{HOME}/Projects/reference/s2disasm-beta8") . '/level';
sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
sub sort_rings {
    my $d = shift;
    my @e;
    for (my $i = 0; $i + 2 <= length $d; $i += 4) {
        my ($x, $yw) = unpack('nn', substr($d, $i, 4) . "\0\0");
        last if $x == 0xFFFF;
        my $vertical = $yw & 0x8000;
        for my $k (0 .. (($yw >> 12) & 7)) {
            push @e, [ $x + ($vertical ? 0 : $k * 0x18), ($yw & 0xFFF) + ($vertical ? $k * 0x18 : 0), scalar @e ];
        }
    }
    return join('', map { pack('nn', $_->[0], $_->[1]) } sort { $a->[0] <=> $b->[0] or $a->[2] <=> $b->[2] } @e) . pack('nn', 0xFFFF, 0);
}
for my $act (1, 2) {
    my $o = slurp("$src/objects/EHZ_$act.bin");
    my $end = 0; # (the end mark is on a 6-byte entry boundary)
    $end += 6 while $end + 6 <= length $o && unpack('n', substr($o, $end, 2)) != 0xFFFF;
    spit("res/S2Objects/EHZ$act", substr($o, 0, $end) . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("res/S2Rings/EHZ$act", sort_rings(slurp("$src/rings/EHZ_$act.bin")));
    printf "EHZ%d: %d objects, %d rings\n", $act, $end / 6, (length(slurp("res/S2Rings/EHZ$act")) - 4) / 4;
}
