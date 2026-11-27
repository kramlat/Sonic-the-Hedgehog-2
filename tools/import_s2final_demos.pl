#!/usr/bin/perl
# Brings the final game's attract mode demos into res/Demo/: Emerald Hill (Sonic's pad, and Tails' for the split screen), Chemical Plant, Aquatic Ruin and Casino Night. The disassembly writes them as
# `demoinput BUTTONS, DURATION` lines; a pair is the buttons held (up, down, left, right, B, C, A, Start from bit 0) and the frames less one. S2FINAL gives the disassembly's folder, default ~/Projects/reference/s2disasm.
# Run it in the Sonic 2 folder: import_s2final_demos.pl
use strict;
use warnings;
my $src = ($ENV{S2FINAL} // "$ENV{HOME}/Projects/reference/s2disasm") . '/s2.asm';
open my $f, '<', $src or die "$src: $!\n";
my @lines = <$f>;
close $f;
my %bit = (U => 0, D => 1, L => 2, R => 3, B => 4, C => 5, A => 6, S => 7);
my %out = (Demo_EHZ => 'FinalEHZ', Demo_EHZ_Tails => 'FinalEHZTails', Demo_CNZ => 'FinalCNZ', Demo_CPZ => 'FinalCPZ', Demo_ARZ => 'FinalARZ');
my ($cur, %data);
for (@lines) {
    if (/^(\w+):/) { $cur = exists $out{$1} ? $1 : undef; next }
    next unless defined $cur;
    if (/^\s*demoinput\s+([A-Z]*)\s*,\s*(\$?[0-9A-Fa-f]+)/) {
        my ($b, $d) = ($1, $2);
        my $mask = 0; $mask |= 1 << $bit{$_} for split //, $b;
        $d = $d =~ /^\$(.*)/ ? hex $1 : $d + 0;
        $data{$cur} .= pack('CC', $mask, $d - 1);
    }
}
for my $label (sort keys %out) {
    die "no data for $label\n" unless defined $data{$label};
    my $d = $data{$label} . pack('CC', 0, 0xFF) x 128; # (the level ends the demo: past its last pair the pad is idle, so the data is a full $100 bytes as the prototypes' were)
    $d = substr($d, 0, 256);
    open my $o, '>:raw', "res/Demo/$out{$label}" or die $!;
    print $o $d;
    close $o;
    printf "%s: %d bytes\n", $out{$label}, length $data{$label};
}
