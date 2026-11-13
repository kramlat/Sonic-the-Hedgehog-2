#!/usr/bin/perl
# Turns a stretch of dc.b / dc.w / dc.l data of a Sonic 2 pre-beta source (from the line of one label up to, not including, the line of another) into its binary, big-endian as the ROM has it. A value is a number ($hex,
# decimal), a difference of two labels (as the offset tables have), or, in a dc.l, a label that is named for its address (Offset_0x0C853E is $0C853E).
#   usage: asm_dc_region.pl <source> <first label> <end label> <output>
use strict;
use warnings;
my ($src, $from, $to, $out) = @ARGV;
die "usage: $0 SOURCE FROM TO OUTPUT\n" unless defined $out;
open my $f, '<', $src or die "$src: $!\n";
my (@items, %label, $pos, $on);
$pos = 0;
while (my $line = <$f>) {
    $line =~ s/\r?\n$//;
    $line =~ s/;.*//;
    if ($line =~ /^(\w+):/) {
        last if $on && $1 eq $to;
        $on = 1 if $1 eq $from;
        $label{$1} = $pos if $on;
        $line =~ s/^\w+:\s*//;
    }
    next unless $on;
    next unless $line =~ /^\s*dc\.([bwl])\s+(.*\S)\s*$/;
    my ($size, $list) = ($1, $2);
    my $width = { b => 1, w => 2, l => 4 }->{$size};
    for my $v (split /\s*,\s*/, $list) {
        push @items, [$width, $v, $pos];
        $pos += $width;
    }
}
die "label $from not found\n" unless $on;
sub value {
    my ($v, $width) = @_;
    return hex($1) if $v =~ /^\$([0-9A-Fa-f]+)$/;
    return $v + 0 if $v =~ /^-?\d+$/;
    if ($v =~ /^(\w+)-(\w+)$/) {
        die "unknown label in $v\n" unless exists $label{$1} && exists $label{$2};
        return $label{$1} - $label{$2};
    }
    return hex($1) if $width == 4 && $v =~ /^Offset_0x([0-9A-Fa-f]+)$/;
    die "cannot read the value '$v'\n";
}
my $data = '';
for my $i (@items) {
    my ($width, $v, $at) = @$i;
    my $n = value($v, $width) & (2**(8 * $width) - 1);
    $data .= pack({ 1 => 'C', 2 => 'n', 4 => 'N' }->{$width}, $n);
}
open my $o, '>:raw', $out or die "$out: $!\n";
print $o $data;
close $o;
printf "%s: %d bytes\n", $out, length $data;
