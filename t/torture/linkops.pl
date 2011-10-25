my $orig = 'MULTIPLE-RENAMES';

open(ORIG, ">$orig") || die "$orig: $!";
print ORIG "FILE CONTENTS\n";
close(ORIG);

rename($orig, "$orig.Xa");
rename("$orig.Xa", "$orig.X");

unlink("UNLINK.X");
