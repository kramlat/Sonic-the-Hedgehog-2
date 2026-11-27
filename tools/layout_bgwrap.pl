#!/usr/bin/perl
# Turns the "background wraps at its own size" flags on or off in layout resources: bit 15 of the background's width word (wraps sideways) and bit 15 of its height word (wraps up and down), words 2 and 3 of the header (LevelLayout.h); in (res/S2Layout/*: Kosinski compressed Sonic 3 format layouts).
# A background that has been made smaller than the level needs (4 x 2 chunks, say) shows nothing beyond its size unless it wraps: a layout saved without the flag does exactly that.
#   usage: layout_bgwrap.pl [--off] [--x|--y] <kosdec> <kosenc> <layout file>...      (the files are rewritten in place only when the flag changes; each is reported)
use strict;
use warnings;
my ($off, $ax, $ay) = (0, 0, 0);
while (@ARGV && $ARGV[0] =~ /^--(off|x|y)$/) { my $o = shift; if ($o eq '--off') { $off = 1 } elsif ($o eq '--x') { $ax = 1 } else { $ay = 1 } }
($ax, $ay) = (1, 1) unless $ax || $ay; # (both axes unless one is named)
my ($kosdec, $kosenc, @files) = @ARGV;
die "usage: $0 [--off] <kosdec> <kosenc> <layout file>...\n" unless defined $kosenc && @files;
for my $f (@files) {
    my $tmp = "/tmp/layout_bgwrap_$$";
    system($kosdec, $f, "$tmp.raw") == 0 or die "kosdec $f failed\n";
    open my $in, '<:raw', "$tmp.raw" or die $!;
    local $/;
    my $d = <$in>;
    close $in;
    my $wword = unpack('n', substr($d, 2, 2)); # (width, with its flag)
    my $hword = unpack('n', substr($d, 6, 2)); # (height, with its flag)
    my ($width, $rows) = ($wword & 0x7FFF, $hword & 0x7FFF);
    my ($nx, $ny) = ($ax ? !$off : ($wword & 0x8000) != 0, $ay ? !$off : ($hword & 0x8000) != 0);
    my ($wn, $hn) = ($width | ($nx ? 0x8000 : 0), $rows | ($ny ? 0x8000 : 0));
    my $state = sprintf('x %s, y %s', $nx ? 'wraps' : 'off', $ny ? 'wraps' : 'off');
    if ($wn == $wword && $hn == $hword) { printf "%s: background %dx%d already %s\n", $f, $width, $rows, $state; next }
    substr($d, 2, 2) = pack('n', $wn);
    substr($d, 6, 2) = pack('n', $hn);
    open my $out, '>:raw', "$tmp.raw" or die $!;
    print $out $d;
    close $out;
    system($kosenc, "$tmp.raw", "$tmp.kos") == 0 or die "kosenc $f failed\n";
    open my $k, '<:raw', "$tmp.kos" or die $!;
    my $c = <$k>;
    close $k;
    open my $w, '>:raw', $f or die $!;
    print $w $c;
    close $w;
    printf "%s: background %dx%d now %s\n", $f, $width, $rows, $state;
}
unlink "/tmp/layout_bgwrap_$$.raw", "/tmp/layout_bgwrap_$$.kos";
