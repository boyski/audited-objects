package CommandAction;

use strict;

use File::Basename;

use constant MSWIN => $^O =~ /MSWin32|Windows_NT/i ? 1 : 0;
use constant CA_FIELDNUM => 14;

sub new {
    my $proto = shift;
    my $self = {};
    bless $self, $proto;

    my $csv = shift;
    my @fields = split(',', $csv, CA_FIELDNUM);
    $self->{CA_PROG} = $fields[8];
    $self->{CA_RWD} = $fields[9];
    $self->{CA_LINE} = $fields[13];
    $self->{CA_TARGETS} = {};
    $self->{CA_PREREQS} = {};

    return $self;
}

sub get_line {
    my $self = shift;
    return $self->{CA_LINE};
}

sub get_prog {
    my $self = shift;
    return $self->{CA_PROG};
}

sub get_rwd {
    my $self = shift;
    return $self->{CA_RWD};
}

sub add_target {
    my $self = shift;
    my $target = shift;
    $self->{CA_TARGETS}->{$target->get_path} = $target;
    return;
}

sub add_prereq {
    my $self = shift;
    my $prereq = shift;
    $self->{CA_PREREQS}->{$prereq->get_path} = $prereq;
}

sub get_prereqs {
    my $self = shift;
    my @prqs;
    for (sort keys %{$self->{CA_PREREQS}}) {
	push(@prqs, $self->{CA_PREREQS}->{$_});
    }
    return @prqs;
}

sub get_targets {
    my $self = shift;
    my @tgts;
    for (keys %{$self->{CA_TARGETS}}) {
	push(@tgts, $self->{CA_TARGETS}->{$_});
    }
    @tgts = sort for_primary @tgts;
    return @tgts;
}

# Sometimes it's helpful to treat one of the targets as "primary".
# There's some black magic to finding this. First we have
# some special cases: directories are pushed to the back of the
# list as are files whose name starts with ".". Files newly
# created are preferred over files merely appended to.
#
# Next we observe that generated "frill" files (.pdb and so on) tend
# to be distinguished by additional extensions or placement in
# subdirectories, and therefore we push shorter paths towards the front.
# After this the file at the front of the line is treated as the primary.
#
# Still, this is basically guesswork and subject to breakage.
# Always looking for a better way.
sub for_primary {
    my $result = 0;
    my($a_wins, $a_loses) = (-1, 1);
    if ($a->is_dir && ! $b->is_dir) {
	$result = $a_loses;
    } else {
	if ($a->is_append && !$b->is_append) {
	    $result = $a_loses;
	} elsif ($b->is_append && !$a->is_append) {
	    $result = $a_wins;
	} else {
	    my $path_a = $a->get_path;
	    my $path_b = $b->get_path;
	    my $a_len = index(basename($path_a), '.');
	    my $b_len = index(basename($path_b), '.');
	    if ($a_len <= 0 && $b_len > 0) {
		$result = $a_loses;
	    } elsif ($b_len <= 0 && $a_len > 0) {
		$result = $a_wins;
	    } else {
		$result = $a_len - $b_len;
		if ($result == 0) {
		    $result = length($path_a) - length($path_b);
		    if ($result == 0) {
			# when all else fails
			$result = $path_a cmp $path_b;
		    }
		}
	    }
	}
    }
    return $result;
}

1;

__END__

=head1 NAME

AO::CommandAction - Represent a particular instantiation of a command line

=head1 SYNOPSIS


=head1 DESCRIPTION

Parses the AO standard command-line CSV format into its component
parts and maintains an object representing that particular command.
This class has close relatives in both the AO client and server,
such that they would all need to be changed if the CSV format or
semantics changed.

=head1 COPYRIGHT

Copyright (c) 2005-2011 David Boyce.  All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
