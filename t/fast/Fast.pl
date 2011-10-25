#!/usr/local/bin/perl

my @names = qw(a b c d e f g h i j k l m n o p q r s t u v w x y z);

open(FAST, ">Fast.c") || die $!;
print FAST "#include <stdio.h>\n\n";
for (@names) {
    print FAST "extern char *$_;\n";
}

print FAST "\nint main(void) {\n";
for (@names) {
    print FAST '    printf("', $_, '=%s\n", ', $_, ");\n";
}
print FAST "}\n";

close(FAST);

for (@names) {
    open(C, ">$_.c") || die "$_.c: $!";
    printf C qq(char *$_ = "%s";\n), uc($_);
    close(C);
}
