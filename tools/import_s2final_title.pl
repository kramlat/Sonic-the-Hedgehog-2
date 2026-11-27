#!/usr/bin/perl
# Brings the final Sonic 2's title screen data into res/: its Nemesis art, Enigma plane maps, palettes, the sprite mappings of the intro (Obj0E) and the menu (Obj0F), and the intro's animation scripts.
# S2FINAL gives the disassembly's folder, default ~/Projects/reference/s2disasm (Beta 8's is the same for these). The mappings are converted by tools/asm2map.pl's asm_to_mappings.
# Run it in the Sonic 2 folder: import_s2final_title.pl
use strict;
use warnings;
my $src = $ENV{S2FINAL} // "$ENV{HOME}/Projects/reference/s2disasm";
sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
mkdir "res/S2TitleF";
my %copy = (
    'res/Art/TitleFMain'      => 'art/nemesis/Main patterns from title screen.nem',
    'res/Art/TitleFSprites'   => 'art/nemesis/Sonic and Tails from title screen.nem',
    'res/Art/TitleFMenu'      => 'art/nemesis/A few menu blocks.nem',
    'res/Art/TitleFPlayers'   => 'art/nemesis/1Player2VS.nem',
    'res/Art/TitleFFont'      => 'art/nemesis/Standard font.nem',
    'res/S2TitleF/MapBG'      => 'mappings/misc/Mappings for title screen background.eni',
    'res/S2TitleF/MapBack'    => 'mappings/misc/Mappings for title screen background 2.eni',
    'res/S2TitleF/MapLogo'    => 'mappings/misc/Sonic the Hedgehog 2 title screen logo mappings.eni',
    'res/S2TitleF/PalTitle'   => 'art/palettes/Title screen.bin',
    'res/S2TitleF/PalSonic'   => 'art/palettes/Title Sonic.bin',
    'res/S2TitleF/PalBack'    => 'art/palettes/Title Background.bin',
    'res/S2TitleF/PalEmblem'  => 'art/palettes/Title Emblem.bin',
    'res/S2TitleF/PalStar'    => 'art/palettes/Title Star Cycle.bin',
);
spit($_, slurp("$src/$copy{$_}")) for sort keys %copy;
# the mappings (tools/asm2map.pl as a library)
require './tools/asm2map.pl';
for my $m (['TitleIntro', 'obj0E'], ['TitleMenuF', 'obj0F']) {
    spit("res/Mappings/$m->[0]", asm_to_mappings(slurp("$src/mappings/sprite/$m->[1].asm")));
}
# Ani_obj0E: Sonic, Tails, the flashing star and the falling star (Sonic 2's script bytes: the delay, the frames, $FA: the second routine counter on). The fixBugs-off version, as the game plays it ($08 is in Sonic's)
my @anim = (
    [1, 5, 6, 7, 8, 0xFA],
    [1, 0, 1, 2, 3, 4, 0xFA],
    [1, 0xC, 0xD, 0xE, 0xD, 0xC, 0xFA],
    [3, 0xC, 0xF, 0xFF],
);
my ($table, $data) = ('', ''); my $pos = 2 * @anim;
for my $a (@anim) { $table .= pack('n', $pos + length $data); $data .= pack('C*', @$a); $data .= "\0" if length($data) & 1 }
spit('res/Animation/TitleIntro', $table . $data);
print "done\n";
