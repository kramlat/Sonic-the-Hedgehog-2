#!/usr/bin/perl
# Turns a sprite mappings table of a Sonic 2 pre-beta object source (dc.w offsets from the table's label, then each frame's dc.w count and dc.l pairs: the disassembly's own layout, Sonic 2's piece format) into the
# resource's binary. A table whose single frame is written under the table's label (no offset list) is not handled: the pre-beta's all have the list.
#   usage: asm_dc_mappings.pl <source> <table label> <output>
use strict;
use warnings;
my ($src, $table, $out) = @ARGV;
die "usage: $0 SOURCE TABLE OUTPUT\n" unless defined $out;
open my $f, '<', $src or die "$src: $!\n";
my @lines = <$f>;
close $f;
s/;.*// for @lines;
my (%body, $current);
for (@lines) {
    if (/^(\w+):?\s*$/) { $current = $1; $body{$current} = []; next }
    push @{ $body{$current} }, $_ if defined $current;
}
my @names = map { /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/ ? $1 : () } @{ $body{$table} // die "no table $table in $src\n" };
die "$table has no frames\n" unless @names;
my $pos = 2 * @names;
my (@offsets, $data);
$data = '';
for my $n (@names) {
    if ($n eq $table) { push @offsets, 0; next } # (the table itself is the frame: its count word is the table's first offset, 0)
    my @l = grep { /\S/ } @{ $body{$n} // die "no frame $n\n" };
    $l[0] =~ /^\s*dc\.w\s+\$([0-9A-Fa-f]+)\s*$/ or die "$n: no count\n";
    my $count = hex $1;
    # the longs, one pair for each piece (a line holds one pair in the pre-beta's source, in Beta 4's several; the values are written with or without leading zeros)
    my @longs;
    for my $i (1 .. $#l) {
        $l[$i] =~ /^\s*dc\.l\s+(.*?)\s*$/ or last;
        push @longs, map { /^\$([0-9A-Fa-f]{1,8})$/ ? hex $1 : /^(\d+)$/ ? $1 + 0 : die "$n: $l[$i]\n" } split /\s*,\s*/, $1;
        last if @longs >= 2 * $count;
    }
    my @pieces;
    push @pieces, pack('NN', splice @longs, 0, 2) while @pieces < $count && @longs >= 2;
    die "$n: $count pieces, found " . scalar @pieces . "\n" unless @pieces == $count;
    my $frame = pack('n', $count) . join('', @pieces);
    push @offsets, $pos;
    $pos += length $frame;
    $data .= $frame;
}
open my $o, '>:raw', $out or die "$out: $!\n";
print $o pack('n*', @offsets) . $data;
close $o;
