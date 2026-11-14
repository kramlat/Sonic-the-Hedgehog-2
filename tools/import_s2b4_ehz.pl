#!/usr/bin/perl
# Brings Sonic 2 Beta 4's Green Hill (Emerald Hill, EHZ, here) layouts, objects and rings into res/ (S2B4 gives the disassembly's folder, default ~/Projects/reference/s2b4-disasm; the files are its data/ghz and
# levels). Its tiles, blocks, chunks, collision and palette are the pre-beta's byte for byte (checked 2026-11-14: Hill Top shares the primary art, chunks and collision with this zone, so nothing of those changes);
# what differs is the layouts (a dozen bytes each), the objects (nine and ten more in the two acts: monitors, springs, spikes, more checkpoints ($79), in the second act an end panel ($0D) and the capsule ($3E)) and the rings.
# The formats and the conversions are tools/import_s2prebeta_ghz.pl's. Needs the kosenc and kosdec tools (KOSENC, KOSDEC give their paths).
# Run it in the Sonic 2 folder: import_s2b4_ehz.pl
use strict;
use warnings;
use File::Temp qw(tempdir);

my $src = ($ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm") . '/data/ghz';
my ($kosenc, $kosdec) = map { $ENV{$_->[0]} // $_->[1] } (['KOSENC', 'kosenc'], ['KOSDEC', 'kosdec']);
my $res = 'res';
my $tmp = tempdir(CLEANUP => 1);
sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
sub run { system(@_) == 0 or die "failed: @_\n" }
sub kos { my ($p, $d) = @_; spit("$tmp/k.raw", $d); run($kosenc, "$tmp/k.raw", $p) }
sub unkos { my $p = shift; run($kosdec, $p, "$tmp/u.raw"); slurp("$tmp/u.raw") }

# The layout is the Sonic 3 & Knuckles format (engine LevelLayout.h): a header (the planes' chunks per row and rows), $40 row pointers ($8000 and the offset in the file), the foreground rows then the background
# rows, each plane only as wide and as tall as what is not empty ($blob is 16 rows of $100 bytes, the foreground first in each and the background from +$80: as tools/import_s2sw_levels.pl's s3_layout)
sub s3_layout {
    my $blob = shift;
    my %w = (0 => 0, 1 => 0); my %h = (0 => 0, 1 => 0);
    for my $plane (0, 1) { for my $row (0 .. 15) { for my $col (0 .. 0x7F) {
        next if ord(substr($blob, $row * 0x100 + $plane * 0x80 + $col, 1)) == 0;
        $w{$plane} = $col + 1 if $col + 1 > $w{$plane}; $h{$plane} = $row + 1 if $row + 1 > $h{$plane};
    } } }
    my $head = pack('n4', $w{0}, $w{1}, $h{0}, $h{1}); my @ptr = (0) x 0x40; my $rows = '';
    for my $plane (0, 1) { for my $row (0 .. $h{$plane} - 1) {
        $ptr[$row * 2 + $plane] = 0x8000 + 8 + 0x80 + length $rows;
        $rows .= substr($blob, $row * 0x100 + $plane * 0x80, $w{$plane});
    } }
    return $head . pack('n*', @ptr) . $rows;
}
# The ring layouts are groups (a line of up to 8 rings in an entry); the ring handler (Rings.h) wants one entry for each ring, in order by X (as tools/import_s2sw_levels.pl's sort_rings), with a 4-byte end mark
sub sort_rings {
    my $d = shift;
    my @e;
    for (my $i = 0; $i + 2 <= length $d; $i += 4) {
        my ($x, $yw) = unpack('nn', substr($d, $i, 4) . "\0\0");
        last if $x == 0xFFFF;
        my $vertical = $yw & 0x8000;
        for my $k (0 .. (($yw >> 12) & 7)) {
            push @e, [ $x + ($vertical ? 0 : $k * 0x18), ($yw & 0xFFF) + ($vertical ? $k * 0x18 : 0), scalar @e ];
        }
    }
    return join('', map { pack('nn', $_->[0], $_->[1]) } sort { $a->[0] <=> $b->[0] or $a->[2] <=> $b->[2] } @e) . pack('nn', 0xFFFF, 0);
}


for my $act (1, 2) {
    my $blob = unkos("$src/GHz_Map$act.kos");
    die "act $act: the layout is not 16 rows\n" unless length $blob == 16 * 0x100;
    kos("$res/S2Layout/EHZ$act", s3_layout($blob));
    my $o = slurp("$src/Obj_Act$act.dat");
    $o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
    spit("$res/S2Objects/EHZ$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("$res/S2Rings/EHZ$act", sort_rings(slurp("$src/ghzRng_Act$act.dat")));
}
print "Emerald Hill (Beta 4's Green Hill): layouts, objects and rings imported\n";
