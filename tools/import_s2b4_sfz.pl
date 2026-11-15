#!/usr/bin/perl
# Brings Sonic 2 Beta 4's Wing Fortress (SFZ, the old Sky Fortress) layout, objects, rings and chunks into res/ (S2B4 gives the disassembly's folder, default ~/Projects/reference/s2b4-disasm; the files are its data/sfz;
# one layout, one object list and one ring list for both acts, as the pre-beta's importer has it). The conversions are tools/import_s2b4_ehz.pl's. Needs kosenc and kosdec (KOSENC, KOSDEC give their paths).
# Run it in the Sonic 2 folder: import_s2b4_sfz.pl
use strict;
use warnings;
use File::Temp qw(tempdir);

my $src = ($ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm") . '/data/sfz';
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


my $blob = unkos("$src/SFz_Map.Kos");
die "the layout is not 16 rows\n" unless length $blob == 16 * 0x100;
my $o = slurp("$src/Obj_Act.dat");
$o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
for my $act (1, 2) {
    kos("$res/S2Layout/SFZ$act", s3_layout($blob));
    spit("$res/S2Objects/SFZ$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("$res/S2Rings/SFZ$act", sort_rings(slurp("$src/sfzRng_Act.dat")));
}
spit("$res/S2Map128/SFZ", slurp("$src/SFz_128.Kos"));
print "Wing Fortress (Beta 4's): layout, objects, rings and chunks imported\n";
