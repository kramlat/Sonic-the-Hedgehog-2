#!/usr/bin/perl
# Brings Sonic 2 Beta 4's Casino Night (CNZ) into res/ (S2B4 gives the disassembly's folder, default ~/Projects/reference/s2b4-disasm; the files are its data/cnz). Compared on 2026-11-14 with what the port had
# from the pre-beta: the tiles, blocks, the slot machine's art and the object art (except the Crawl) are the same; what differs is the chunks, the collision, the palette, the layouts' conversion inputs, the
# objects, the rings, the large triangle bumpers' lists and the Crawl's art. The conversions are tools/import_s2prebeta_zone.pl's. Needs kosenc, kosdec and solids2s3 (KOSENC, KOSDEC, SOLIDS2S3 give their paths).
# Run it in the Sonic 2 folder: import_s2b4_cnz.pl
use strict;
use warnings;
use File::Temp qw(tempdir);

my $src = ($ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm") . '/data/cnz';
my ($kosenc, $kosdec, $solids2s3) = map { $ENV{$_->[0]} // $_->[1] } (['KOSENC', 'kosenc'], ['KOSDEC', 'kosdec'], ['SOLIDS2S3', 'solids2s3']);
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
    my $blob = unkos("$src/CNz_Map$act.kos");
    die "act $act: the layout is not 16 rows\n" unless length $blob == 16 * 0x100;
    kos("$res/S2Layout/CNZ$act", s3_layout($blob));
    my $o = slurp("$src/Obj_Act$act.dat");
    $o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
    spit("$res/S2Objects/CNZ$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("$res/S2Rings/CNZ$act", sort_rings(slurp("$src/cnzRng_Act$act.dat")));
    spit("$res/S2Triangles/CNZ$act", slurp("$src/Tri_Act$act.dat"));
}
spit("$res/S2Map128/CNZ", slurp("$src/CNz_128.kos"));
spit("$res/S2Palette/CNZ", slurp("$src/CNz.pal"));
kos("$res/S2Collision/CNZ1", slurp("$src/CNz_Col1.dat"));
kos("$res/S2Collision/CNZ2", slurp("$src/CNz_Col2.dat"));
run($solids2s3, "$res/S2Collision/CNZ", "$res/S2Collision/CNZ1", "$res/S2Collision/CNZ2");
unlink "$res/S2Collision/CNZ1", "$res/S2Collision/CNZ2";
spit("$res/Art/CNZCrawl", slurp("$src/Crawl.nem"));

# the objects' mappings and animations (Sonic 2's format: the build converts the mappings): the tools are the pre-beta importer's, on Beta 4's sources (objects/obj_0xNN.asm; the labels are the same)
my $objects = ($ENV{S2B4} // "$ENV{HOME}/Projects/reference/s2b4-disasm") . '/objects';
for my $job (
    [ 'asm_dc_mappings.pl', 'D8', 'Green_Bumper_Mappings', 'res/Mappings/CNZGreenBumper' ],
    [ 'asm_dc_animation.pl', 'D8', 'Green_Bumper_Animate_Data', 'res/Animation/CNZGreenBumper' ],
    [ 'asm_dc_mappings.pl', '85', 'Vertical_Pinball_Spring_Mappings', 'res/Mappings/CNZPlungerDiagonal' ], # (the names are the other way round: this one is used with a subtype)
    [ 'asm_dc_mappings.pl', '85', 'Diagonal_Pinball_Spring_Mappings', 'res/Mappings/CNZPlunger' ],
    [ 'asm_dc_mappings.pl', 'D5', 'CNz_Elevator_Mappings', 'res/Mappings/CNZElevator' ],
    [ 'asm_dc_mappings.pl', 'D4', 'Blue_Box_Mappings', 'res/Mappings/CNZBlueBox' ],
    [ 'asm_dc_mappings.pl', 'D2', 'Green_Platform_Mappings', 'res/Mappings/CNZGreenPlatform' ],
    [ 'asm_dc_mappings.pl', 'C8', 'Crawl_Mappings', 'res/Mappings/CNZCrawl' ],
    [ 'asm_dc_animation.pl', 'C8', 'Crawl_Animate_Data', 'res/Animation/CNZCrawl' ],
    [ 'asm_dc_mappings.pl', 'D7', 'Blue_Bumper_Mappings', 'res/Mappings/CNZBlueBumper' ],
    [ 'asm_dc_animation.pl', 'D7', 'Blue_Bumper_Animate_Data', 'res/Animation/CNZBlueBumper' ],
    [ 'asm_dc_mappings.pl', 'D6', 'Slot_Machine_Starter_Mappings', 'res/Mappings/CNZSlotStarter' ],
    [ 'asm_dc_animation.pl', 'D6', 'Slot_Machine_Starter_Animate_Data', 'res/Animation/CNZSlotStarter' ],
    [ 'asm_dc_mappings.pl', '86', 'Flippers_Mappings', 'res/Mappings/CNZFlippers' ],
    [ 'asm_dc_animation.pl', '86', 'Flippers_Animate_Data', 'res/Animation/CNZFlippers' ],
) {
    run('perl', "tools/$job->[0]", "$objects/obj_0x$job->[1].asm", $job->[2], $job->[3]);
}
print "Casino Night (Beta 4's) imported\n";
