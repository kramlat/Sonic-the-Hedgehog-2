#!/usr/bin/perl
# Brings Sonic 2 Beta 4's Chemical Plant (CPZ) over what the port has from the pre-beta (S2B4 gives the disassembly's folder, default ~/Projects/reference/s2b4-disasm). Compared on 2026-11-14 (decompressed, byte
# for byte): the tiles, blocks, collision, layouts, rings and the sprite art are the pre-beta's; what differs is the chunks (156 bytes: Death Egg shares this zone's tiles, blocks and chunks, so it has them too), the
# two palettes (the zone's and the water's) and the objects (act 1: 144 against 140, act 2: 180 against 159: more pipe tippers ($0B), monitors ($26), spikes ($36), spirals ($6B), checkpoints ($79), and the capsule ($3E)).
# Written: res/S2Map128/CPZ, res/S2Palette/CPZ and CPZWater, res/S2Objects/CPZ1 and CPZ2. Run it in the Sonic 2 folder: import_s2b4_cpz.pl
use strict;
use warnings;

my $b4 = $ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm";
my $res = 'res';
sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }

spit("$res/S2Map128/CPZ", slurp("$b4/map128/CPz_128.kos"));       # (Kosinski already)
spit("$res/S2Palette/CPZ", slurp("$b4/data/cpz/CPz.pal"));
spit("$res/S2Palette/CPZWater", slurp("$b4/data/cpz/CPz_UW.pal"));
for my $act (1, 2) {
    my $o = slurp("$b4/data/cpz/Obj_Act$act.dat");
    $o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
    spit("$res/S2Objects/CPZ$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
}
print "Chemical Plant (Beta 4's): chunks, palettes and objects imported\n";
