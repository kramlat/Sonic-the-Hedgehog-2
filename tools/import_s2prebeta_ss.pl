#!/usr/bin/perl
# The Sonic 2 pre-beta's special stage data (S2PREBETA gives the disassembly's folder, default ~/Projects/reference/s2prebeta-disasm; its Data/SS) into res, under the names the port has:
#   art (Nemesis, kept as it is): res/Art/SS<Name>; the main palette: res/S2Palette/SpecialStage; the track's segment data and the two other blobs: res/SSData/<ROM offset>
# The data's files are named for what they are (the ROM offsets they have in the pre-beta, in the names map below: SegmentNN is the stage's NNth segment, in the order of its table at $6D5A).
# The two lists the stage loads (PLC $3C and $3D) and the tile numbers are in Sonic2PLC.c and Object/BossArt.h's neighbour SpecialStageArt.h. The leftovers of an earlier build (PB_*) are not imported.
# Run it in the Sonic 2 folder: import_s2prebeta_ss.pl ; it prints the lines for paradoxmakefile.yml's resources
use strict;
use warnings;
use File::Copy;

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/reference/s2prebeta-disasm") . '/Data/SS';
my %art = (
    'Sonic.nem' => 'SpecialStageSonic', 'Sonic_D.nem' => 'SpecialStageSonicDiagonal', 'Sonic_H.nem' => 'SpecialStageSonicHorizontal', 'Sonic_S.nem' => 'SpecialStageSonicSpin',
    'Miles.nem' => 'SpecialStageMiles', 'Miles_D.nem' => 'SpecialStageMilesDiagonal', 'Miles_H.nem' => 'SpecialStageMilesHorizontal', 'Miles_S.nem' => 'SpecialStageMilesSpin',
    'Miles_T.nem' => 'SpecialStageMilesTail', 'Miles_TD.nem' => 'SpecialStageMilesTailDiagonal', 'Miles_TH.nem' => 'SpecialStageMilesTailHorizontal',
    'Backgrnd.nem' => 'SpecialStageBackground', 'Hud_Font.nem' => 'SpecialStageHudFont', 'TrafficL.nem' => 'SpecialStageTrafficLights', 'Rings.nem' => 'SpecialStageRings',
    'P_Shadow.nem' => 'SpecialStagePlayerShadow', 'D_Shadow.nem' => 'SpecialStageDiagonalShadow', 'V_Shadow.nem' => 'SpecialStageVerticalShadow',
    'BombsExp.nem' => 'SpecialStageBombsExplosion', 'Bombs.nem' => 'SpecialStageBombs', 'Emerald.nem' => 'SpecialStageEmerald', 'WingHand.nem' => 'SpecialStageWingsHand',
);
my %names = (
    '0011B8' => 'PacePatterns',
    '006CFC' => 'SegmentLists',
    '006E3A' => 'TileWords',
    '007302' => 'RunTiles',
    '00751A' => 'TrackBlocks',
    '008E5C' => 'VerticalCurves',
    '008FEC' => 'HorizontalCurves',
    '02F94E' => 'CharacterArtScript',
    '0309C8' => 'ObjectLayouts',
    '0C853E' => 'Segment00',
    '0C89E2' => 'Segment01',
    '0C8FB0' => 'Segment02',
    '0C9568' => 'Segment03',
    '0C9BCC' => 'Segment04',
    '0CA1F8' => 'Segment05',
    '0CA8B4' => 'Segment06',
    '0CAEBC' => 'Segment07',
    '0CB3FA' => 'Segment08',
    '0CB97E' => 'Segment09',
    '0CBEF8' => 'Segment0A',
    '0CC418' => 'Segment0B',
    '0CC88C' => 'Segment0C',
    '0CCCF6' => 'Segment0D',
    '0CD1BA' => 'Segment0E',
    '0CD63A' => 'Segment0F',
    '0CDA84' => 'Segment10',
    '0CDEC6' => 'Segment11',
    '0CE544' => 'Segment12',
    '0CEAE0' => 'Segment13',
    '0CF03A' => 'Segment14',
    '0CF636' => 'Segment15',
    '0CFAE6' => 'Segment16',
    '0CFFE8' => 'Segment17',
    '0D0400' => 'Segment18',
    '0D084E' => 'Segment19',
    '0D0CCC' => 'Segment1A',
    '0D115C' => 'Segment1B',
    '0D1626' => 'Segment1C',
    '0D1BB2' => 'Segment1D',
    '0D229A' => 'Segment1E',
    '0D29E0' => 'Segment1F',
    '0D3036' => 'Segment20',
    '0D3592' => 'Segment21',
    '0D3B3C' => 'Segment22',
    '0D41D0' => 'Segment23',
    '0D47E4' => 'Segment24',
    '0D4C68' => 'Segment25',
    '0D50D6' => 'Segment26',
    '0D554C' => 'Segment27',
    '0D59E4' => 'Segment28',
    '0D5E8A' => 'Segment29',
    '0D6232' => 'Segment2A',
    '0D6626' => 'Segment2B',
    '0D6A5E' => 'Segment2C',
    '0D6EF0' => 'Segment2D',
    '0D73B2' => 'Segment2E',
    '0D77BA' => 'Segment2F',
    '0D7C50' => 'Segment30',
    '0D8108' => 'Segment31',
    '0D875A' => 'Segment32',
    '0D8CC0' => 'Segment33',
    '0D91E8' => 'Segment34',
    '0D979C' => 'Segment35',
    '0D9D8E' => 'Segment36',
    '0DA222' => 'Segment37',
    '0DA672' => 'TrackTiles',
    '0DCA9C' => 'BackgroundMapBottom',
    '0DCBCA' => 'BackgroundMapTop',
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
    $n = $names{$n} // die "no name for $n\n";
    copy("$src/$f", "res/SSData/$n") or die "$f: $!\n";
    push @yml, "SSData/$n";
}
print "  - \"$_\"\n" for @yml;
