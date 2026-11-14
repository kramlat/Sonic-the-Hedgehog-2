#!/usr/bin/perl
# Imports the cards of Sonic 2 Beta 4 (s2b4: desiresin's Sonic-2-Beta-4-Improved, S2B4 gives the folder, default ~/Projects/s2b4-disasm): the mappings of the end of act card
# (End_Level_Results_Mappings, object $3A) as asm/Mappings/LevelResults.asm (Sonic 2 mappings format, the pieces as the disassembly has them), and the cards' Nemesis art
# (res/Art/B4CardZone: ZONE and the fixed letters at tile $580; B4CardResults: the results' words at $5B0; B4MiniSonic: the continue icon at $5F4).
# Run it in the Sonic 2 folder: import_s2b4_cards.pl
use strict;
use warnings;

my $src = $ENV{S2B4} // "$ENV{HOME}/Projects/s2b4-disasm";
my $asm = do { open my $f, '<', "$src/Sonic2B4.asm" or die "$src/Sonic2B4.asm: $!\n"; local $/; <$f> };

# the pieces of a frame, by label: [ [4 words] ... ]
sub frame_of {
    my ($label) = @_;
    $asm =~ /^\Q$label\E:[^\n]*\n\s*dc\.w\s+\$([0-9A-Fa-f]+)[^\n]*\n((?:\s*dc\.l\s+[^\n]*\n)+)/m or die "no frame $label\n";
    my ($count, $body) = (hex $1, $2);
    my @longs = map { hex } ($body =~ /\$([0-9A-Fa-f]+)/g);
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

write_mappings('asm/Mappings/LevelResults.asm', 'End_Level_Results_Mappings', 'The end of act card (object $3A)',
    'Frames: 0-2 the first line, 3 and 4 the second line, 5 ZONE, 6-8 the act numbers, 9 TIME BONUS, 10 RING BONUS, 11 the total, 12 and 13 the continue icon (flashing)');

for my $x (['artnem/TC_ZONE.nem', 'B4CardZone'], ['artnem/TC_Res.nem', 'B4CardResults'], ['artnem/M_Sonic.nem', 'B4MiniSonic']) {
    open my $in, '<:raw', "$src/$x->[0]" or die "$x->[0]: $!\n"; local $/; my $d = <$in>; close $in;
    open my $out, '>:raw', "res/Art/$x->[1]" or die; print $out $d; close $out;
}
