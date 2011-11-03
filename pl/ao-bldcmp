#!/usr/bin/perl

use Cwd 'getcwd';
use File::Find;
use String::CRC32;

my($refdir, $updir, $dndir) = @ARGV;
my %files;

sub path_hash {
    my $path = shift;
    return 0 unless -f $path;
    my($phash) = split ' ', qx(ao stat -s '$path');
    return $phash || 0;
}

sub wanted_ref {
    return unless -f;
    my $path = $File::Find::name;
    $files{$path} = path_hash($_);
}

sub wanted_up {
    return unless -f;
    my $path = $File::Find::name;
    if (exists $files{$path}) {
	my $hash = path_hash($_);
	if ($files{$path} ne $hash) {
	    warn "$0: Warning: ref file difference: $path\n";
	    delete $files{$path};
	}
    } else {
	warn "$0: Warning: ref path difference: $path\n";
    }
}

sub wanted_dn {
    return unless -f;
    my $path = $File::Find::name;
    if (exists $files{$path}) {
	my $hash = path_hash($_);
	delete $files{$path} if $files{$path} eq $hash;
    }
}

my $cwd = getcwd;

chdir $refdir || die "$0: Error: $refdir: $!\n";
find(\&wanted_ref, '.');
chdir $cwd || die "$0: Error: $cwd: $!\n";

chdir $updir || die "$0: Error: $updir: $!\n";
find(\&wanted_up, '.');
chdir $cwd || die "$0: Error: $cwd: $!\n";

chdir $dndir || die "$0: Error: $dndir: $!\n";
find(\&wanted_dn, '.');
chdir $cwd || die "$0: Error: $cwd: $!\n";

for my $path (keys %files) {
    warn "$0: Warning: down path difference: $path\n";
}
