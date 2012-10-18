for my $bin (@ARGV) {
    my $loc = qx(which $bin 2>/dev/null);
    system("which $bin >/dev/null 2>&1 &");
}
