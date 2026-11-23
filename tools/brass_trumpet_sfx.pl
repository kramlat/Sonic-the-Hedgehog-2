#!/usr/bin/perl
# Writes the Brass Eggman's trumpet blasts as SMPS2ASM effect sources (the mobile port's Egg Trumpet plays blasts at several notes): seven notes, each its own effect with its own voice bank (the importer does not
# understand shared banks), one FM voice made to sound like a brass: two parallel carriers (algorithm 4), the first with a fast, bright attack from its modulator's feedback, the second an octave up and later, and a small
# vibrato that comes in after the attack. Convert them with tools/import_s2_music.pl --s1-tempo <asm2json> <this folder> <output folder>
#   usage: brass_trumpet_sfx.pl <output folder>
use strict;
use warnings;
my $out = shift or die "usage: $0 <output folder>\n";
mkdir $out unless -d $out;
my @notes = (['C4', 'nC4'], ['D4', 'nD4'], ['E4', 'nE4'], ['G4', 'nG4'], ['A4', 'nA4'], ['C5', 'nC5'], ['E5', 'nE5']);
my $i = 0;
for my $n (@notes) {
    my ($name, $const) = @$n;
    $i++;
    my $label = "SndBrass${i}_$name";
    open my $f, '>', "$out/Brass $i - Trumpet $name.asm" or die $!;
    print $f <<"END";
${label}_Header:
	smpsHeaderStartSong 2
	smpsHeaderVoice     ${label}_Voices
	smpsHeaderTempoSFX  \$01
	smpsHeaderChanSFX   \$01

	smpsHeaderSFXChannel cFM5, ${label}_FM5,	\$00, \$00

; FM5 Data
${label}_FM5:
	smpsSetvoice        \$00
	smpsModSet          \$0A, \$01, \$03, \$02
	dc.b	$const, \$28
	smpsStop

${label}_Voices:
;	Voice \$00: a brass: algorithm 4 (the operators 2 and 4 are heard), feedback 5
	smpsVcAlgorithm     \$04
	smpsVcFeedback      \$05
	smpsVcUnusedBits    \$00
	smpsVcDetune        \$03, \$00, \$03, \$00
	smpsVcCoarseFreq    \$01, \$01, \$01, \$02
	smpsVcRateScale     \$00, \$00, \$00, \$00
	smpsVcAttackRate    \$14, \$1C, \$12, \$1A
	smpsVcAmpMod        \$00, \$00, \$00, \$00
	smpsVcDecayRate1    \$0A, \$06, \$08, \$07
	smpsVcDecayRate2    \$04, \$03, \$04, \$03
	smpsVcDecayLevel    \$03, \$02, \$03, \$02
	smpsVcReleaseRate   \$06, \$08, \$06, \$08
	smpsVcTotalLevel    \$80, \$0A, \$80, \$14
END
    close $f;
}
