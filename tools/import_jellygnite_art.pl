#!/usr/bin/perl
# Makes the Jellygnite's art and mappings from the Sonic 2 (2013) Hidden Palace objects sheet (sprites ripped by Kurai, sheet by Dolphman: credit Sega and Sonic Team if used) and the user's map picture of how it hangs
# (a dome with three chains of four small beads below it). The sheet has three domes (two grey with green glass, one red) and four beads (grey with green, grey, red, orange); they are cleaned (the sheet's magenta
# and its blur are not art) and drawn in the colours of palette line 0, Sonic's: the greys of its silver ramp (indices 6, 7, 8, 9 and black 1), BLUE in place of the green (the user: "replace the green in it with blue", the
# blue "can be out of Sonic's colors": indices 2 to 5) and the reds and oranges (12 to 15).
# Writes res/Art/Jellygnite (raw tiles: the written to VRAM by the object: 3 domes of 4x3 tiles, then 4 beads of 1 tile), res/Mappings/Jellygnite (3 frames) and res/Animation/Jellygnite.
#   usage: import_jellygnite_art.pl <the sheet's image (png or ppm)>   (run in the Sonic 2 folder; it needs ImageMagick's `magick` to read a png)
use strict;
use warnings;

my $src = shift or die "usage: $0 <sheet image>\n";
my $ppm = $src;
if ($src !~ /\.ppm$/i) {
    $ppm = "/tmp/jellygnite_sheet_$$.ppm";
    system('magick', $src, '-depth', '8', $ppm) == 0 or die "magick failed\n";
}
open my $in, '<:raw', $ppm or die "$ppm: $!\n";
local $/;
my $data = <$in>;
close $in;
$data =~ s/^P6\s+(\d+)\s+(\d+)\s+(\d+)\s// or die "not a P6 ppm\n";
my ($W, $H) = ($1, $2);
sub px { my ($x, $y) = @_; my $i = ($y * $W + $x) * 3; return unpack('C3', substr($data, $i, 3)) }

# the sheet's cells: where the three domes and four beads are (found by looking: the sheet is 576 pixels wide)
my @dome_x = (283, 314, 344);
my ($dome_y0, $dome_w, $dome_h) = (56, 29, 18);
my @bead_x = (315, 320, 328, 334);
my ($bead_y0) = (75);

# what a sheet colour is in palette line 0 (0 is transparent)
sub index_of {
    my ($r, $g, $b) = @_;
    my $max = $r > $g ? ($r > $b ? $r : $b) : ($g > $b ? $g : $b);
    return 0 if $r > 150 && $b > 150 && $g < 165 && abs($r - $b) < 80;       # the sheet's magenta, and its blurred edge
    return 0 if abs($r - 149) < 20 && abs($g - 177) < 20 && abs($b - 200) < 20; # the sheet's own light blue background
    return 1 if $max < 26;                                                    # black
    if (abs($r - $g) < 14 && abs($g - $b) < 14) {                             # greys: the silver ramp
        my $l = ($r + $g + $b) / 3;
        return $l > 200 ? 6 : $l > 150 ? 7 : $l > 100 ? 8 : $l > 50 ? 9 : 1;
    }
    if ($g > $r + 12 && $g > $b + 12) {                                       # the green becomes blue
        return $g < 45 ? 2 : $g < 100 ? 3 : $g < 130 ? 4 : 5;
    }
    if ($r > $g + 40 && $r > $b + 40) {                                       # reds
        return $r < 100 ? 13 : 12;
    }
    if ($r > $b + 40) {                                                       # oranges and yellows
        return $g > $r * 0.7 ? 14 : 15;
    }
    my $l = ($r + $g + $b) / 3;                                               # anything else: the nearest grey
    return $l > 200 ? 6 : $l > 150 ? 7 : $l > 100 ? 8 : 9;
}

# a canvas of palette indices, 32x24 for a dome and 8x8 for a bead
sub canvas { my ($w, $h) = @_; return [ map { [ (0) x $w ] } 1 .. $h ] }
my @dome;
for my $f (0 .. 2) {
    my $c = canvas(32, 24);
    for my $y (0 .. $dome_h - 1) {
        for my $x (0 .. $dome_w - 1) {
            $c->[$y + 3]->[$x + 1] = index_of(px($dome_x[$f] + $x, $dome_y0 + $y));
        }
    }
    # the domes are drawn a little off one another on the sheet: each is put in the middle of the canvas, its top at the same row
    my ($minx, $maxx) = (99, -1);
    for my $y (0 .. 23) { for my $x (0 .. 31) { next unless $c->[$y]->[$x]; $minx = $x if $x < $minx; $maxx = $x if $x > $maxx } }
    my $shift = 16 - int(($minx + $maxx + 1) / 2);
    if ($shift) {
        my $m = canvas(32, 24);
        for my $y (0 .. 23) { for my $x (0 .. 31) { my $nx = $x + $shift; $m->[$y]->[$nx] = $c->[$y]->[$x] if $nx >= 0 && $nx < 32 } }
        $c = $m;
    }
    push @dome, $c;
}
my @bead;
for my $b (0 .. 3) {
    my $c = canvas(8, 8);
    for my $y (0 .. 5) {
        for my $x (0 .. 5) {
            $c->[$y + 1]->[$x + 1] = index_of(px($bead_x[$b] + $x, $bead_y0 + $y));
        }
    }
    push @bead, $c;
}

# the tiles: a 4x3 dome in the order the VDP draws a sprite's pieces (a column at a time), a bead one tile
sub tile {
    my ($c, $tx, $ty) = @_;
    my $out = '';
    for my $y (0 .. 7) {
        for my $x (0 .. 3) {
            $out .= chr(($c->[$ty * 8 + $y]->[$tx * 8 + $x * 2] << 4) | $c->[$ty * 8 + $y]->[$tx * 8 + $x * 2 + 1]);
        }
    }
    return $out;
}
my $art = '';
for my $f (0 .. 2) {
    for my $tx (0 .. 3) { for my $ty (0 .. 2) { $art .= tile($dome[$f], $tx, $ty) } }
}
$art .= tile($_, 0, 0) for @bead;
open my $o, '>:raw', 'res/Art/Jellygnite' or die $!;
print $o $art;
close $o;

# a look at what was made (the indices as characters, for checking)
if ($ENV{JELLY_SHOW}) {
    my @ch = ('.', '#', 'a', 'b', 'c', 'd', 'W', 'S', 'G', 'D', 'e', 'f', 'R', 'r', 'Y', 'O');
    for my $f (0 .. 2) { print "dome $f\n"; print join('', map { $ch[$_] } @$_), "\n" for @{ $dome[$f] } }
    for my $b (0 .. 3) { print "bead $b\n"; print join('', map { $ch[$_] } @$_), "\n" for @{ $bead[$b] } }
}

# the mappings (the 8-byte pieces: y, size, tile word, the two player tile word, x). A frame: the dome (4x3 tiles) and twelve beads: three chains of four, hanging below it, which sway from one frame to the next
# (the user's map: a dome about 30x20 with the chains at its left, middle and right)
my @chain_a = ([-12, 13], [-11, 19], [-10, 25], [-7, 31], [1, 15], [-1, 21], [-2, 26], [-3, 32], [13, 13], [11, 18], [10, 24], [7, 31]);
my @chain_b = ([-12, 13], [-12, 19], [-12, 25], [-11, 31], [1, 15], [1, 21], [1, 26], [0, 32], [13, 13], [13, 18], [13, 24], [12, 31]);
sub piece { my ($y, $size, $tile, $x) = @_; return pack('cCnnn', $y, $size, $tile, $tile, $x & 0xFFFF) }
my @frames;
for my $f (0 .. 2) {
    my $first_tile = 12 * $f;
    my $bead_tile = 36 + ($f == 2 ? 2 : $f);
    my $chain = $f == 1 ? \@chain_b : \@chain_a;
    my $body = piece(-14, 0x0E, $first_tile, -16);
    my $n = 1;
    for my $p (@$chain) {
        $body .= piece($p->[1] - 4 - 9, 0, $bead_tile, $p->[0] - 4);
        $n++;
    }
    push @frames, pack('n', $n) . $body;
}
my $map = '';
my $offset = 2 * @frames;
my @offs;
for my $fr (@frames) { push @offs, $offset; $offset += length $fr }
$map .= pack('n', $_) for @offs;
$map .= $_ for @frames;
open $o, '>:raw', 'res/Mappings/Jellygnite' or die $!;
print $o $map;
close $o;

# the animations: 0 hovering (the dome and the chains sway), 1 the count (the red dome flashes)
my @scripts = ([ 0x0B, 0, 1, 0xFF ], [ 0x03, 2, 0, 0xFF ]);
my $anim = '';
my $aoffset = 2 * @scripts;
my @aoffs;
for my $s (@scripts) { push @aoffs, $aoffset; $aoffset += scalar @$s }
$anim .= pack('n', $_) for @aoffs;
$anim .= join('', map { chr } @$_) for @scripts;
open $o, '>:raw', 'res/Animation/Jellygnite' or die $!;
print $o $anim;
close $o;
print "Jellygnite: ", length($art) / 32, " tiles, ", scalar(@frames), " frames\n";
