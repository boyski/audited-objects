#!/usr/bin/perl

sub run {
    print STDERR "+ @_\n";
    exit(2) if system(@_);
}

run(qw( gcc -c hw.c));
run(qw( gcc -o hw hw.o));
run(qw( gcc -c hw.c));
run(qw( gcc -o hw hw.o));
