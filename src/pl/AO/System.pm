package main;

$VERSION = '1.00';
@ISA = qw(Exporter);
@EXPORT = qw(run syserr);

use strict;
require Exporter;
use File::Basename;

use vars qw($keepgoing $prog $verbosity);

($prog) = fileparse($0, '\.\w*$');
$keepgoing = 0;
$verbosity = 0;

sub showcmd {
    print STDERR "+ @_\n" if $verbosity > 0;
}

sub run {
    my $ret;
    if (!defined($_[0]) || ref($_[0]) eq 'SCALAR') {
	$ret = shift;
    }
    @_ = grep length, @_;
    showcmd(@_);
    if (defined(wantarray)) {
	if (wantarray) {
	    my @results = qx(@_);
	    $$ret = $? >> 8 if $ret;
	    return @results;
	} else {
	    my $results = qx(@_);
	    $$ret = $? >> 8 if $ret;
	    return $results;
	}
    } else {
	if(system(@_)) {
	    $$ret = $? >> 8 if $ret;
	    exit(2) unless $keepgoing;
	} else {
	    $$ret = 0 if $ret;
	}
	return 0;
    }
}

# Report a system error, potentially fatal.
sub syserr {
    my $fatal = shift;
    my $terms = join(' or ', @_);
    if ($fatal) {
	die "$prog: Error: $terms: $!\n";
    } else {
	warn "$prog: Warning: $terms: $!\n";
    }
}

1;
