package CommandAction;

use strict;

use constant MSWIN => $^O =~ /MSWin32|Windows_NT/i ? 1 : 0;
use constant CA_FIELDNUM => 13;

sub new {
    my $proto = shift;
    my $self = {};
    bless $self, $proto;

    my $csv = shift;
    my @fields = split(',', $csv, CA_FIELDNUM);
    $self->{CA_RWD} = $fields[8];
    $self->{CA_LINE} = $fields[12];
    $self->{CA_TARGETS} = {};
    $self->{CA_PREREQS} = {};

    return $self;
}

sub get_line {
    my $self = shift;
    return $self->{CA_LINE};
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
    for (sort keys %{$self->{CA_TARGETS}}) {
	push(@tgts, $self->{CA_TARGETS}->{$_});
    }
    return @tgts;
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

Copyright (c) 2005-2010 David Boyce.  All rights reserved.

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
