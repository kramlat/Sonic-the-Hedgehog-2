#!/usr/bin/perl
# The Flasher and the Crawlton of Dust Hill in the Sonic 2 pre-beta (objects $A3 and $9E: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into res/, from its Data/Objects/Obj_0xA3.asm,
# Obj_0x9E.asm and Data/DHz/Flasher.nem:
#   Art/Flasher             the Flasher's art (Nemesis; it is not the alpha's: five frames' worth of tiles)
#   Mappings/Flasher        its five frames (Sonic 2's format: the build converts them)
#   Mappings/Crawlton       the head (frames 0 and 1 are one body) and a piece of the body (frame 2)
#   Animation/Flasher       the flashing, Animation/FlasherAlert the glow, Animation/FlasherFade the fade (each a script of its own: the object runs all three as its animation 0)
# The Crawlton's art (Data/DHz/Crawlton.nem) is the alpha's, byte for byte.
# Run it in the Sonic 2 folder: import_s2prebeta_dhzbadniks.pl
use strict;
use warnings;

my $base = $ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm";

sub slurp { my $p = shift; open my $in, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$in>; close $in; $d }
sub spit { my ($p, $d) = @_; open my $o, '>:raw', $p or die "$p: $!\n"; print $o $d; close $o }

# the labels of a source and their lines, comments off
sub labels {
    my ($file) = @_;
    open my $f, '<:encoding(latin1)', $file or die "$file: $!\n";
    my (%body, $current);
    while (<$f>) {
        s/;.*//;
        if (/^(\w+):?\s*$/) { $current = $1; $body{$current} = []; next }
        push @{ $body{$current} }, $_ if defined $current;
    }
    close $f;
    return \%body;
}

# a mappings table: its dc.w entries are offsets from its start (frames that share a label share a body)
sub mappings {
    my ($body, $table) = @_;
    my @names = map { /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/ ? $1 : () } @{ $body->{$table} // die "no table $table\n" };
    my $pos = 2 * @names;
    my (@offsets, $data, %at);
    $data = '';
    for my $n (@names) {
        if (exists $at{$n}) { push @offsets, $at{$n}; next }
        my @l = grep { /\S/ } @{ $body->{$n} // die "no frame $n\n" };
        my ($count) = $l[0] =~ /^\s*dc\.w\s+\$([0-9A-Fa-f]+)\s*$/ or die "$n: no count\n";
        $count = hex $count;
        my @pieces;
        for my $i (1 .. $#l) {
            last if @pieces == $count;
            $l[$i] =~ /^\s*dc\.l\s+\$([0-9A-Fa-f]{8})\s*,\s*\$([0-9A-Fa-f]{8})/ or die "$n: $l[$i]\n";
            push @pieces, pack('NN', hex $1, hex $2);
        }
        die "$n: $count pieces, found " . scalar @pieces . "\n" unless @pieces == $count;
        my $frame = pack('n', $count) . join('', @pieces);
        $at{$n} = $pos;
        push @offsets, $pos;
        $pos += length $frame;
        $data .= $frame;
    }
    return pack('n*', @offsets) . $data;
}

# an animation of one script: the table's single entry points at the script's dc.b lines
sub script {
    my ($body, $table) = @_;
    my @names = map { /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/ ? $1 : () } @{ $body->{$table} // die "no table $table\n" };
    die "$table: one script expected\n" unless @names == 1;
    my $bytes = '';
    for (@{ $body->{ $names[0] } }) {
        next unless /^\s*dc\.b\s+(.*\S)/;
        $bytes .= pack('C*', map { hex } map { s/^\s*\$//r } split /,/, $1);
    }
    $bytes =~ s/\xFC\x00\z/\xFC/; # (the byte of padding after the script's end command)
    return pack('n', 2) . $bytes;
}

my $flasher = labels("$base/Data/Objects/Obj_0xA3.asm");
my $crawlton = labels("$base/Data/Objects/Obj_0x9E.asm");
spit('res/Art/Flasher', slurp("$base/Data/DHz/Flasher.nem"));
spit('res/Mappings/Flasher', mappings($flasher, 'Flasher_Mappings'));
spit('res/Mappings/Crawlton', mappings($crawlton, 'Crawlton_Mappings'));
spit('res/Animation/Flasher', script($flasher, 'Flasher_Animate_Data'));
spit('res/Animation/FlasherAlert', script($flasher, 'Flasher_Animate_Data_01'));
spit('res/Animation/FlasherFade', script($flasher, 'Flasher_Animate_Data_02'));
print "Dust Hill's Flasher and Crawlton: art, mappings and animations written\n";
