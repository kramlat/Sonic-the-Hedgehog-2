#!/usr/bin/perl
# The pre-beta's object hit (the explosion of a destroyed badnik or monitor, object $27) and its art: S2PREBETA gives the disassembly's folder (default ~/Projects/s2prebeta-disasm).
#   res/Art/ObjectHit        Data/Art/Explosn.nem (loaded at tile $5A4 by the pre-beta's list $02)
#   res/Mappings/ObjectHit   Object_Hit_Mappings from Data/Objects/Obj_0x27.asm (Sonic 2's format: the build converts them)
# Run it in the Sonic 2 folder: import_s2prebeta_objecthit.pl
use strict;
use warnings;

my $base = $ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm";
system('cp', "$base/Data/Art/Explosn.nem", 'res/Art/ObjectHit') == 0 or die "art\n";
system('perl', 'tools/asm_dc_mappings.pl', "$base/Data/Objects/Obj_0x27.asm", 'Object_Hit_Mappings', 'res/Mappings/ObjectHit') == 0 or die "mappings\n";
print "object hit imported\n";
