#!/usr/bin/perl
# Brings the final game's objects and rings of zones into res/S2Objects/<CODE>1-2 and res/S2Rings/<CODE>1-2 (Emerald Hill and Chemical Plant: their foreground layouts are byte for byte ours, checked 2026-11-27, so the
# layouts, and the backgrounds that have been optimized, are not touched). S2FINAL gives the disassembly's folder, default ~/Projects/reference/s2disasm (Beta 8's has the same objects).
#   usage: import_s2final_level.pl EHZ CPZ ...      (the zone codes as the files in res/ have them)
# The object layout is the engine's format already (six bytes an object, the end mark FFFF); the ring layout is groups (a line of up to 8 rings in an entry) and the ring handler (Rings.h) wants one entry for each
# ring in order by X, with a 4-byte end mark (as tools/import_s2b4_ehz.pl's sort_rings).
# Run it in the Sonic 2 folder: import_s2b8_ehz.pl
use strict;
use warnings;

my $src = ($ENV{S2FINAL} // "$ENV{HOME}/Projects/reference/s2disasm") . '/level';
die "usage: $0 CODE...\n" unless @ARGV;
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
for my $code (@ARGV) { for my $act (1, 2) {
    my $o = slurp("$src/objects/${code}_$act.bin");
    my $end = 0; # (the end mark is on a 6-byte entry boundary)
    $end += 6 while $end + 6 <= length $o && unpack('n', substr($o, $end, 2)) != 0xFFFF;
    spit("res/S2Objects/$code$act", substr($o, 0, $end) . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("res/S2Rings/$code$act", sort_rings(slurp("$src/rings/${code}_$act.bin")));
    printf "%s%d: %d objects, %d rings\n", $code, $act, $end / 6, (length(slurp("res/S2Rings/$code$act")) - 4) / 4;
}
}
