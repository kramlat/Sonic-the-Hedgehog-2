#!/usr/bin/perl
# The badniks of Oil Ocean in the Sonic 2 pre-beta (the Octus, object $4A, and the Aquis, $50: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into res/, from its Data/Objects/Obj_0x4A.asm
# and Obj_0x50.asm and Data/OOz/Octus.nem and Aquis.nem:
#   Art/Octus, Art/Aquis             the art (Nemesis)
#   Mappings/Octus, Mappings/Aquis   the frames (Sonic 2's format: the build converts them)
#   Animation/Octus, Animation/Aquis the animation scripts (one table of several scripts each)
# Run it in the Sonic 2 folder: import_s2prebeta_oozbadniks.pl
use strict;
use warnings;

my $base = $ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm";

sub slurp { my $p = shift; open my $in, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$in>; close $in; $d }
sub spit { my ($p, $d) = @_; open my $o, '>:raw', $p or die "$p: $!\n"; print $o $d; close $o }

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

sub entries { my ($body, $table) = @_; map { /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/ ? $1 : () } @{ $body->{$table} // die "no table $table\n" } }

sub mappings {
    my ($body, $table) = @_;
    my @names = entries($body, $table);
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

# an animation: the offsets, then each script's bytes (the dc.b lines under its label)
sub animation {
    my ($body, $table) = @_;
    my @names = entries($body, $table);
    my $pos = 2 * @names;
    my (@offsets, $data);
    $data = '';
    for my $n (@names) {
        my $bytes = '';
        for (@{ $body->{$n} }) {
            next unless /^\s*dc\.b\s+(.*\S)/;
            $bytes .= pack('C*', map { hex } map { s/^\s*\$//r } split /,/, $1);
        }
        push @offsets, $pos;
        $pos += length $bytes;
        $data .= $bytes;
    }
    return pack('n*', @offsets) . $data;
}

my $octus = labels("$base/Data/Objects/Obj_0x4A.asm");
spit('res/Art/Octus', slurp("$base/Data/OOz/Octus.nem"));
spit('res/Mappings/Octus', mappings($octus, 'Octus_Mappings'));
spit('res/Animation/Octus', animation($octus, 'Octus_AnimateData'));
my $aquis = labels("$base/Data/Objects/Obj_0x50.asm");
spit('res/Art/Aquis', slurp("$base/Data/OOz/Aquis.nem"));
spit('res/Mappings/Aquis', mappings($aquis, 'Aquis_Mappings'));
spit('res/Animation/Aquis', animation($aquis, 'Aquis_Animate_Data'));
print "Oil Ocean's Octus and Aquis: art, mappings and animations written\n";
