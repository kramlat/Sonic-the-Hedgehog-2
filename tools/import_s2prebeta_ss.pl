#!/usr/bin/perl
# The Sonic 2 pre-beta's special stage data (S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm; its Data/SS) into res, under the names the port has:
#   art (Nemesis, kept as it is): res/Art/SS<Name>; the main palette: res/S2Palette/SpecialStage; the track's segment data and the two other blobs: res/SSData/<ROM offset>
# The two lists the stage loads (PLC $3C and $3D) and the tile numbers are in Sonic2PLC.c and Object/BossArt.h's neighbour SpecialStageArt.h. The leftovers of an earlier build (PB_*) are not imported.
# Run it in the Sonic 2 folder: import_s2prebeta_ss.pl ; it prints the lines for paradoxmakefile.yml's resources
use strict;
use warnings;
use File::Copy;

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . '/Data/SS';
my %art = (
    'Sonic.nem' => 'SpecialStageSonic', 'Sonic_D.nem' => 'SpecialStageSonicDiagonal', 'Sonic_H.nem' => 'SpecialStageSonicHorizontal', 'Sonic_S.nem' => 'SpecialStageSonicSpin',
    'Miles.nem' => 'SpecialStageMiles', 'Miles_D.nem' => 'SpecialStageMilesDiagonal', 'Miles_H.nem' => 'SpecialStageMilesHorizontal', 'Miles_S.nem' => 'SpecialStageMilesSpin',
    'Miles_T.nem' => 'SpecialStageMilesTail', 'Miles_TD.nem' => 'SpecialStageMilesTailDiagonal', 'Miles_TH.nem' => 'SpecialStageMilesTailHorizontal',
    'Backgrnd.nem' => 'SpecialStageBackground', 'Hud_Font.nem' => 'SpecialStageHudFont', 'TrafficL.nem' => 'SpecialStageTrafficLights', 'Rings.nem' => 'SpecialStageRings',
    'P_Shadow.nem' => 'SpecialStagePlayerShadow', 'D_Shadow.nem' => 'SpecialStageDiagonalShadow', 'V_Shadow.nem' => 'SpecialStageVerticalShadow',
    'BombsExp.nem' => 'SpecialStageBombsExplosion', 'Bombs.nem' => 'SpecialStageBombs', 'Emerald.nem' => 'SpecialStageEmerald', 'WingHand.nem' => 'SpecialStageWingsHand',
);
mkdir 'res/SSData';
my @yml;
for my $f (sort keys %art) {
    copy("$src/$f", "res/Art/$art{$f}") or die "$f: $!\n";
    push @yml, "Art/$art{$f}";
}
copy("$src/SS_Main.pal", 'res/S2Palette/SpecialStage') or die "SS_Main.pal: $!\n";
push @yml, 'S2Palette/SpecialStage';
opendir my $d, $src or die "$src: $!\n";
for my $f (sort grep { /^0x[0-9A-F]+\.dat$/ } readdir $d) {
    (my $n = $f) =~ s/^0x//;
    $n =~ s/\.dat$//;
    copy("$src/$f", "res/SSData/$n") or die "$f: $!\n";
    push @yml, "SSData/$n";
}
print "  - \"$_\"\n" for @yml;
