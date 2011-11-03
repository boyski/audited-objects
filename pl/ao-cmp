#!/usr/bin/perl

# op,call,seq,pid,depth,ppid,pccode,ccode,fs,nmtime,size,mode,dcode,path

# Compares two raw AO output files, showing the files which differ.

my $left = shift;
my $right = shift;

my %Left;
open($left, $left) || die "Error: $left: $!";
for (<$left>) {
    next unless m%^[A-Z]%;
    chomp;
    my @fields = split ',';
    next unless $fields[0] =~ m%^[A-Z]%;
    my $path = $fields[-1];
    $path =~ s%^[a-z]:%%i;
    if (exists($Left{$path}) && $Left{$path} ne $fields[-2]) {
	warn "Warning: multiple left versions: $path\n";
    }
    $Left{$path} = $fields[-2];
}
close($left);

my %Right;
open($right, $right) || die "Error: $right: $!";
for (<$right>) {
    next unless m%^[A-Z]%;
    chomp;
    my @fields = split ',';
    my $path = $fields[-1];
    $path =~ s%^[a-z]:%%i;
    if (exists($Right{$path}) && $Right{$path} ne $fields[-2]) {
	warn "Warning: multiple right versions: $path\n";
    }
    $Right{$path} = $fields[-2];
}
close($right);

for (sort keys %Right) {
    if (!exists($Left{$_})) {
	warn "Warning: $_ seen only in $right\n";
	next;
    }
}

for (sort keys %Left) {
    if (!exists($Right{$_})) {
	warn "Warning: $_ seen only in $left\n";
	next;
    }
    if ($Left{$_} ne $Right{$_}) {
	warn "Warning: DIFFERENCE: $_\n";
    } else {
	#print "SAME: $_\n";
    }
}
