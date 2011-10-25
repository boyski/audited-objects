# Creates a file of size (2 ** A) + B where A and B are args 1 and 2.

my $power = shift;
my $delta = shift;

my $size = (2 ** $power) + $delta;

my $lf = "LARGE.X";

open(LARGE, ">$lf") || die "$lf: $!";

while ($size > 0) {
    my $len = $size > 80 ? 80 : $size;
    $size -= $len;
    print LARGE 'X' x ($len - 1), "\n";
}

close(LARGE);
