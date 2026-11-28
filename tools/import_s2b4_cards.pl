#!/usr/bin/perl
# Imports the cards of Sonic 2 Beta 4 (s2b4: desiresin's Sonic-2-Beta-4-Improved, S2B4 gives the folder, default ~/Projects/reference/s2b4-disasm): the mappings of the end of act card
# (End_Level_Results_Mappings, object $3A) as asm/Mappings/LevelResults.asm (Sonic 2 mappings format, the pieces as the disassembly has them), and the cards' Nemesis art
# (res/Art/B4CardZone: ZONE and the fixed letters at tile $580; B4CardResults: the results' words at $5B0; B4MiniSonic: the continue icon at $5F4).
# Run it in the Sonic 2 folder: import_s2b4_cards.pl
use strict;
use warnings;

my $src = $ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm";
my $asm = do { open my $f, '<', "$src/Sonic2B4.asm" or die "$src/Sonic2B4.asm: $!\n"; local $/; <$f> };
for my $o ('obj_0x21', 'obj_0x3E', 'obj_0x79') {   # (the versus banner's and the egg prison's mappings are in their objects' files)
    $asm .= do { open my $f, '<', "$src/objects/$o.asm" or die "$o.asm: $!\n"; local $/; "\n" . <$f> };
}

# the pieces of a frame, by label: [ [4 words] ... ]
sub frame_of {
    my ($label) = @_;
    $asm =~ /^\Q$label\E:[^\n]*\n\s*dc\.w\s+\$([0-9A-Fa-f]+)[^\n]*\n((?:\s*dc\.l\s+[^\n]*\n)+)/m or die "no frame $label\n";
    my ($count, $body) = (hex $1, $2);
    my @longs;
    for my $line (split /\n/, $body) {
        $line =~ s/^\s*dc\.l\s+//;
        $line =~ s/;.*//;
        for my $t (split /\s*,\s*/, $line) {
            $t =~ s/\s+$//;
            next if $t eq '';
            push @longs, $t =~ /^\$([0-9A-Fa-f]+)$/ ? hex($1) : $t + 0;
        }
    }
    die "$label: $count pieces but " . scalar(@longs) . " longs\n" unless @longs == 2 * $count;
    my @pieces;
    while (@longs) {
        my ($a, $b) = splice @longs, 0, 2;
        push @pieces, [ $a >> 16, $a & 0xFFFF, $b >> 16, $b & 0xFFFF ];
    }
    return \@pieces;
}

# the frames of a mappings table: its entries' labels
sub table_labels {
    my ($table) = @_;
    $asm =~ /^\Q$table\E:[^\n]*\n((?:\s*dc\.w\s+\w+-\Q$table\E[^\n]*\n)+)/m or die "no table $table\n";
    return [ $1 =~ /dc\.w\s+(\w+)-/g ];
}

sub write_mappings {
    my ($file, $table, $title, $frames_note) = @_;
    my $labels = table_labels($table);
    open my $o, '>', $file or die "$file: $!\n";
    print $o "; ---------------------------------------------------------------------------\n; $title: $table of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl\n; $frames_note\n; ---------------------------------------------------------------------------\n.int:";
    print $o "\t\tdc.w .f$_-.int\n" for 0 .. $#$labels;
    for my $i (0 .. $#$labels) {
        my $p = frame_of($labels->[$i]);
        printf $o ".f%d:\t\tdc.w %d\t\t; %s\n", $i, scalar(@$p), $labels->[$i];
        printf $o "\t\tdc.w \$%04X, \$%04X, \$%04X, \$%04X\n", @$_ for @$p;
    }
    close $o;
    printf "%s: %d frames\n", $file, scalar(@$labels);
}

write_mappings('asm/Mappings/VsBanner.asm', '_2P_Versus_Results_Mappings', 'The versus results banner (object $21)', 'Frames: 0 1P WINS, 1 2P WINS, 2 TIED');
write_mappings('asm/Mappings/EggPrison.asm', 'Egg_Prison_Mappings', 'The egg prison (object $3E)', 'Frames: 0-3 the capsule closed to open, 4 the button, 5 the lid');
write_mappings('asm/Mappings/StarPost.asm', 'Star_Post_Mappings', 'The star post (object $79)', 'Frames: 0 the post and its ball, 1 without the ball, 2 the ball, 3 the ball (the other palette), 4 the post with the other palette ball');
write_mappings('asm/Mappings/StarPostStars.asm', 'Star_Post_Stars_Mappings', 'The bonus stars of the star post', 'Frames: 0 a big star, 1 and 2 small ones');
write_mappings('asm/Mappings/LevelResults.asm', 'End_Level_Results_Mappings', 'The end of act card (object $3A)',
    'Frames: 0-2 the first line, 3 and 4 the second line, 5 ZONE, 6-8 the act numbers, 9 TIME BONUS, 10 RING BONUS, 11 the total, 12 and 13 the continue icon (flashing)');

for my $x (['artnem/VsResFnt.nem', 'S2Menu/VsResultFont'], ['mapeni/VsActRes.eni', 'S2Menu/VsActRes'], ['mapeni/VsLvlRes.eni', 'S2Menu/VsLvlRes'], ['mapeni/VsGblRes.eni', 'S2Menu/VsGblRes'], ['mapeni/Vs_SSRes.eni', 'S2Menu/VsSSRes'], ['mapeni/VsAll_SS.eni', 'S2Menu/VsAllSS'], ['artnem/EggPrisn.nem', 'EggPrison'], ['artnem/StarPost.nem', 'StarPost'], ['artnem/TC_ZONE.nem', 'B4CardZone'], ['artnem/TC_Res.nem', 'B4CardResults'], ['artnem/M_Sonic.nem', 'B4MiniSonic']) {
    open my $in, '<:raw', "$src/$x->[0]" or die "$x->[0]: $!\n"; local $/; my $d = <$in>; close $in;
    my $dest = $x->[1] =~ m{/} ? "res/$x->[1]" : "res/Art/$x->[1]";
    open my $out, '>:raw', $dest or die "$dest: $!\n"; print $out $d; close $out;
}

# ---- the title cards: the names, ZONE, the act numbers, the banner and the stripes (Title_Cards_Mappings) ----
# The port's name frames are in its own order (TitleCard.c's TitleCard_NameFrame); Beta 4's are by zone id. Zones that Beta 4 has no name for (Wood, Sand Shower, Cyber City) are written from the alphabet.
my $tlabels = table_labels('Title_Cards_Mappings');          # (23 frames)
my %tc_const;                                                 # TC_A = $4 ... : [first tile, tiles]
for my $l (split /\n/, $asm) {
    $tc_const{$1} = [ hex($2) >> 8, hex($2) & 0xFF ] if $l =~ /^TC_(\w)\s*=\s*\$([0-9A-Fa-f]+)/;
}
my %loader;                                                   # Load_GHz: the letters in the order they are copied
while ($asm =~ /^(Load_\w+):\s*\n\s*dc\.w\s+([^\n]+)\n/mg) {
    my ($label, $list) = ($1, $2);
    $loader{$label} = [ map { $tc_const{$_} // die "no TC_$_\n" } ($list =~ /TC_([A-Z])\b/g) ];
}
my @names = (   # [ name, Beta 4's frame (or undef: authored), its loader ]
    [ 'EMERALD HILL',   0, 'Load_GHz' ], [ 'METROPOLIS', 4, 'Load_Mz' ], [ 'HILL TOP', 7, 'Load_HTz' ], [ 'OIL OCEAN', 10, 'Load_OOz' ], [ 'MYSTIC CAVE', 11, 'Load_DHz' ],
    [ 'CASINO NIGHT',  12, 'Load_CNz' ], [ 'CHEMICAL PLANT', 13, 'Load_CPz' ], [ 'AQUATIC RUIN', 15, 'Load_NGHz' ], [ 'SKY CHASE', 16, 'Load_SCz' ], [ 'WING FORTRESS', 6, 'Load_SFz' ],
    [ 'DEATH EGG',     14, 'Load_DEz' ], [ 'WOOD', undef ], [ 'SAND SHOWER', undef ], [ 'CYBER CITY', undef ], [ 'HIDDEN PALACE', 8, 'Load_HPz' ], [ 'ROCK WORLD', undef ], [ 'OCEAN WIND', undef ],
);
my %alpha = map { $_ => $tc_const{$_} } keys %tc_const;
my %fixed = (E => 0x580, N => 0x584, O => 0x588);
my $LETTERS = 0x5DE;
my @tc_frames;      # [ comment, [pieces] ]
my @tc_letters;     # per name: [ first tile, tiles, ... ]
for my $n (@names) {
    my ($name, $bf, $ld) = @$n;
    if (defined $bf) {
        push @tc_frames, [ $name, frame_of($tlabels->[$bf]) ];
        push @tc_letters, [ map { @$_ } @{ $loader{$ld} } ];
    } else {
        my $total = 0;
        my $width = sub { my $c = shift; $c eq ' ' ? 16 : $c eq 'I' ? 8 : ($c eq 'M' || $c eq 'W') ? 24 : 16 };
        $total += $width->($_) for split //, $name;
        my $x = int((0x34 - $total / 2) / 8) * 8;
        my ($next, %tile, @words, @letters) = ($LETTERS);
        for my $c (split //, $name) {
            if ($c ne ' ') {
                my ($t, $size);
                if (exists $fixed{$c}) { $t = $fixed{$c}; $size = 5 }
                else {
                    die "no letter $c\n" unless $alpha{$c};
                    unless (exists $tile{$c}) { $tile{$c} = $next; push @letters, @{ $alpha{$c} }; $next += $alpha{$c}[1] }
                    $t = $tile{$c};
                    $size = $alpha{$c}[1] == 2 ? 1 : $alpha{$c}[1] == 6 ? 9 : 5;
                }
                push @words, [ $size, 0x8000 | $t, 0x8200 | ($t >> 1), $x & 0xFFFF ];
            }
            $x += $width->($c);
        }
        push @tc_frames, [ $name, \@words ];
        push @tc_letters, \@letters;
    }
}
push @tc_frames, [ 'ZONE', frame_of($tlabels->[17]) ], [ 'ACT 1', frame_of($tlabels->[18]) ], [ 'ACT 2', frame_of($tlabels->[19]) ], [ 'ACT 3', frame_of($tlabels->[20]) ],
                 [ 'BANNER (SONIC THE HEDGEHOG)', frame_of($tlabels->[21]) ], [ 'STRIPES (the zigzag edge)', frame_of($tlabels->[22]) ];
open my $tm, '>', 'asm/Mappings/TitleCardNames.asm' or die;
print $tm "; ---------------------------------------------------------------------------\n; The title cards of Sonic 2 Beta 4 (Sonic 2 mappings format), tools/import_s2b4_cards.pl: the names (in TitleCard.c's order), ZONE, ACT 1-3, the banner and the zigzag edge\n; (Wood, Sand Shower, Cyber City, Rock World and Ocean Wind are written from Beta 4's alphabet: it has no names for them)\n; ---------------------------------------------------------------------------\n.int:";
print $tm "\t\tdc.w .f$_-.int\n" for 0 .. $#tc_frames;
for my $i (0 .. $#tc_frames) {
    my ($c, $w) = @{ $tc_frames[$i] };
    printf $tm ".f%d:\t\tdc.w %d\t\t; %s\n", $i, scalar(@$w), $c;
    printf $tm "\t\tdc.w \$%04X, \$%04X, \$%04X, \$%04X\n", @$_ for @$w;
}
close $tm;
open my $td, '>', 'src/TitleCardData.c' or die;
print $td "// The letters each title card name needs from the alphabet art (Art_TitleCardLetters), as pairs of first tile and tiles, in the order they are copied to tile \$5DE on, ended by \$FF:\n// Beta 4's Load_Title_Cards_Art. Written by tools/import_s2b4_cards.pl (the frames are those of asm/Mappings/TitleCardNames.asm).\n#include <stdint.h>\n\n#define TITLECARD_NAMES 18 // (the frames of the names)\n\n";
printf $td "const uint8_t titlecard_letters[TITLECARD_NAMES][20] = {\n";
for my $i (0 .. $#names) {
    printf $td "    /* %2d %s */ { %s0xFF },\n", $i, $names[$i][0], join('', map { sprintf '0x%02X, ', $_ } @{ $tc_letters[$i] });
}
printf $td "    /* %2d ZONE ZONE */ { 0xFF }, // (not a real zone: the id one past the last slot, a side effect of counting; its name is the ZONE frame, which the fixed art holds)\n", scalar @names;
print $td "};\n";
close $td;
for my $x (['artnem/TC_ZONE.nem', 'TitleCardZone'], ['artnem/TC_AY.nem', 'TitleCardLetters']) {
    open my $in, '<:raw', "$src/$x->[0]" or die "$x->[0]: $!\n"; local $/; my $d = <$in>; close $in;
    open my $out, '>:raw', "res/Art/$x->[1]" or die; print $out $d; close $out;
}
printf "title cards: %d frames\n", scalar(@tc_frames);
