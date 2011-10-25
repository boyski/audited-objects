package PathAction;

use strict;

use File::Basename;
use File::Spec;

use constant MSWIN => $^O =~ /MSWin32|Windows_NT/i ? 1 : 0;
use constant PA_FIELDNUM => 17;

sub new {
    my $proto = shift;
    my $self = {};
    bless $self, $proto;

    my $csv = shift;
    my @fields = split(',', $csv, PA_FIELDNUM);
    $self->{PA_OP} = $fields[0];
    $self->{PA_DCODE} = $fields[14];
    my $path = $fields[-1];
    $path =~ s%/%\\%g if MSWIN;
    if (!File::Spec->file_name_is_absolute($path)) {
	my $basedir = shift;
	$path = File::Spec->join($basedir, $path);
	$self->is_member(1);
    }
    $self->{PA_PATH} = $path;

    # The whole area of symlinks here is complicated. From a
    # filesystem POV there's no need for a symlink to resolve,
    # or ever expect to resolve, to a legal filename. But
    # in practice they almost always do, and when make is
    # involved we can assume they will since files are all make
    # knows about. But note especially that whatever we do with
    # the path here, it will affect only make's DAG; the actual
    # script which creates or opens the symlink will be unaffected.
    # BTW, we have to canonicalize the path because make thinks in
    # terms of pathnames, not inodes, but we cannot use realpath or
    # similar because in the case of a symlink to a symlink it
    # would lose the dependency on the intermediate link.
    if (my $path2 = $fields[-2]) {
	$path2 =~ s%/%\\%g if MSWIN;
	if (!File::Spec->file_name_is_absolute($path2)) {
	    $path2 = File::Spec->join(dirname($path), $path2);
	}
	$path2 =~ s%([\\/])[\\/]+%$1%g;
	while ($path2 =~ s%[^\\/]+[\\/]\.\.[\\/]%%) {}
	$self->{PA_PATH2} = $path2;
    }

    return $self;
}

sub exists {
    my $self = shift;
    return -e $self->{PA_PATH};
}

sub is_target {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op !~ m%^[RXU]%;
}

sub is_prereq {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op =~ m%^[RX]%;
}

sub is_unlink {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op eq 'U';
}

sub is_symlink {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op eq 'S';
}

sub is_dir {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op eq 'D';
}

sub is_append {
    my $self = shift;
    my $op = $self->{PA_OP};
    return $op eq 'A';
}

sub is_member {
    my $self = shift;
    $self->{PA_MEMBER} = shift if @_;
    return exists($self->{PA_MEMBER}) && $self->{PA_MEMBER};
}

sub get_dcode {
    my $self = shift;
    return $self->{PA_DCODE};
}

sub get_path {
    my $self = shift;
    return $self->{PA_PATH};
}

sub get_path2 {
    my $self = shift;
    return exists($self->{PA_PATH2}) ? $self->{PA_PATH2} : undef;
}

1;

__END__

=head1 NAME

AO::PathAction - Represent a particular instantiation of a file access

=head1 SYNOPSIS


=head1 DESCRIPTION

Parses the AO standard file-access CSV format into its component
parts and maintains an object representing that particular access.
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
