#!/usr/bin/perl
# Sonic 2 Beta 4's special stage resources (S2B4 gives the disassembly's folder, default ~/Projects/reference/s2b4-disasm; its data/ss) into res, beside the pre-beta's (the names have the prefix SSB4; what is byte for byte the
# pre-beta's is not copied again: the 56 segments, the background, bombs, explosion, emerald, the three shadows, the two background maps). Prints the lines for paradoxmakefile.yml's resources.
# Run it in the Sonic 2 folder: import_s2b4_ss.pl
use strict;
use warnings;
use File::Copy;

my $src = ($ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm") . '/data/ss';
my %art = (
    'Rings.nem' => 'SSB4Rings', 'HUD_Font.nem' => 'SSB4HudFont', 'WingHand.nem' => 'SSB4WingsHand', 'START.nem' => 'SSB4Start', 'Bg_Stars.nem' => 'SSB4Stars', 'PAYMW_vs.nem' => 'SSB4PaymwVs',
    'SoncMils.nem' => 'SSB4SonicMiles', 'Emeralds.nem' => 'SSB4Emeralds', 'tc_res.nem' => 'SSB4Results',
);
my %pal = (
    'SS_Main.pal' => 'SSB4Main', 'ss_1.pal' => 'SSB4Stage1', 'ss_2.pal' => 'SSB4Stage2', 'ss_3.pal' => 'SSB4Stage3', 'ss_4.pal' => 'SSB4Stage4', 'ss_5.pal' => 'SSB4Stage5', 'ss_6.pal' => 'SSB4Stage6', 'ss_7.pal' => 'SSB4Stage7',
    'SS_Vs_1.pal' => 'SSB4Vs1', 'SS_Vs_2.pal' => 'SSB4Vs2', 'SS_Vs_3.pal' => 'SSB4Vs3', 'SSResult.pal' => 'SSB4Results',
);
my %data = (
    'SS_8.kos' => 'B4TrackTiles', 'SS_Map_0.kos' => 'B4Map0', 'SS_Map_1.kos' => 'B4Map1', 'SS_Map_2.kos' => 'B4Map2', '0x0E16AA.dat' => 'B4Unknown',
);
my @yml;
for my $f (sort keys %art) { copy("$src/$f", "res/Art/$art{$f}") or die "$f: $!\n"; push @yml, "Art/$art{$f}" }
for my $f (sort keys %pal) { copy("$src/$f", "res/S2Palette/$pal{$f}") or die "$f: $!\n"; push @yml, "S2Palette/$pal{$f}" }
for my $f (sort keys %data) { copy("$src/$f", "res/SSData/$data{$f}") or die "$f: $!\n"; push @yml, "SSData/$data{$f}" }
print "  - \"$_\"\n" for @yml;

# The characters' data, read out of the disassembly's source with the asm tools (the rest of the characters' mappings and animations, the shadows' and Miles' tails, are the pre-beta's, byte for byte):
#   Mappings/SSB4Sonic (loc_306FC: one late frame differs from the pre-beta's) and SSData/B4CharacterArtScript (loc_30AE4: 55 entries for Sonic, Miles and his tails; the art is one picture, SSB4SonicMiles)
my $s = $src; $s =~ s{/data/ss$}{};
system('perl', 'tools/asm_dc_mappings.pl', "$s/Sonic2B4.asm", 'loc_306FC', 'res/Mappings/SSB4Sonic') == 0 or die;
system('perl', 'tools/asm_dc_region.pl', "$s/Sonic2B4.asm", 'loc_30AE4', 'loc_30CCC', 'res/SSData/B4CharacterArtScript') == 0 or die;
print "  - \"Mappings/SSB4Sonic\"\n  - \"SSData/B4CharacterArtScript\"\n";

# The rings' and bombs' mappings and animations (loc_324D2, loc_326B0, loc_3247A, loc_32676)
for my $r (['asm_dc_mappings.pl', 'loc_324D2', 'Mappings/SSB4Rings'], ['asm_dc_mappings.pl', 'loc_326B0', 'Mappings/SSB4Bombs'], ['asm_dc_animation.pl', 'loc_3247A', 'Animation/SSB4Rings'], ['asm_dc_animation.pl', 'loc_32676', 'Animation/SSB4Bombs']) {
    system('perl', "tools/$r->[0]", "$s/Sonic2B4.asm", $r->[1], "res/$r->[2]") == 0 or die;
    print "  - \"$r->[2]\"\n";
}

# The HUD, the START banner, the ring counters' digits, the letters of the end's texts and the checkpoint's pictures
for my $r (['Sonic2B4.asm', 'asm_dc_mappings.pl', 'loc_7E76', 'Mappings/SSB4Counter'], ['Sonic2B4.asm', 'asm_dc_mappings.pl', 'loc_31FF2', 'Mappings/SSB4Letters'],
           ['objects/obj_0x5E.asm', 'asm_dc_mappings.pl', 'SS_Head_Up_Display_Mappings', 'Mappings/SSB4Hud'], ['objects/obj_0x5F.asm', 'asm_dc_mappings.pl', 'SS_Start_Flag_Mappings', 'Mappings/SSB4Start'],
           ['objects/obj_0x59.asm', 'asm_dc_mappings.pl', 'loc_32402', 'Mappings/SSB4Checkpoint'], ['objects/obj_0x59.asm', 'asm_dc_animation.pl', 'loc_323D0', 'Animation/SSB4Checkpoint']) {
    system('perl', "tools/$r->[1]", "$s/$r->[0]", $r->[2], "res/$r->[3]") == 0 or die;
    print "  - \"$r->[3]\"\n";
}
