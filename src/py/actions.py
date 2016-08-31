"""Classes to support AO post-processing."""

#############################################################################
# Copyright (c) 2005-2016 David Boyce.  All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#############################################################################

from __future__ import print_function
from __future__ import unicode_literals

import os

CA_SPLITS = 13
PA_SPLITS = 16
SEP = ','


class PathAction(object):

    """Parse the AO file-access CSV format into its component parts."""

    def __init__(self, csv, basedir):
        fields = csv.rstrip().split(SEP, PA_SPLITS)
        self.op = fields[0]
        self.dcode = fields[14]
        self.path2 = fields[-2]
        self.path = fields[-1]
        self.is_member = not os.path.isabs(self.path)
        if self.is_member:
            self.path = os.path.join(basedir, self.path)

        # The whole area of symlinks in AO is complicated. From a
        # filesystem POV there's no need for a symlink to resolve,
        # or even expect to ever resolve, to a legal filename. But
        # in practice they almost always do, and when make is
        # involved we can assume they will since files are all make
        # understands (it would have no idea what to do with a dangling
        # symlink). But note especially that whatever we do with the
        # path here it could affect only make's DAG; the actual script
        # which creates or opens the symlink will be unaffected.
        # BTW, we have to canonicalize the path because make thinks in
        # terms of pathnames, not inodes, but we cannot use realpath or
        # similar because in the case of a symlink to a symlink it
        # would lose the dependency on the intermediate link.

        # The next-to-last slot is reserved for a possible symlink target.
        if self.path2:
            if not os.path.isabs(self.path2):
                self.path2 = os.path.join(
                    os.path.dirname(self.path), self.path2)
            self.path2 = os.path.normpath(self.path2)

    def __cmp__(self, other):
        """

        Sometimes it's helpful to treat one among multiple sibling
        targets as "primary". There's some black magic to deciding which
        that should be. First we have some special cases: directories
        are pushed to the back of the list as are files whose name
        starts with ".". Files newly created are preferred over files
        merely appended to.

        Next we observe that generated "frill" files (.pdb and so on)
        tend to be distinguished by additional extensions or placement
        in subdirectories, so we favor shorter paths.  The target at
        the front of the list after all this is considered primary.

        Still, this is basically guesswork and subject to breakage.
        Always looking for a better way.

        This logic is available for sorting any PA set but is useful
        primarily for targets.
        """
        if self.path == other.path:
            return 0
        elif self.path[0] == '.' and other.path[0] != '.':
            return -1
        elif self.is_dir() and not other.is_dir():
            return -1
        elif self.is_append() and not other.is_append():
            return -1
        else:
            diff = len(self.path) - len(other.path)
            if diff == 0:
                return cmp(self.path, other.path)
            else:
                return -1 if diff > 0 else 1

    def __hash__(self):
        """Required due to cmp override."""
        return hash(self.op + self.path)

    def __repr__(self):
        return ' '.join([self.op, self.path])

    def exists(self):
        """Report whether this path currently exists."""
        return os.path.exists(self.path)

    def is_prereq(self):
        """Report whether this path action represents a prereq read."""
        return self.op in 'RX'

    def is_target(self):
        """Report whether this path action represents a target write."""
        return self.op not in 'RXU'

    def is_append(self):
        """Report whether this path action represents a file append."""
        return self.op == 'A'

    def is_symlink(self):
        """Report whether this path action represents a symlink creation."""
        return self.op == 'S'

    def is_unlink(self):
        """Report whether this path action represents a file removal."""
        return self.op == 'U'

    def is_dir(self):
        """Report whether this path action represents a mkdir."""
        return self.op == 'D'


class CommandAction(object):

    """Parse the AO command-line CSV format into its component parts."""

    def __init__(self, csv):
        fields = csv.rstrip().split(SEP, CA_SPLITS)
        self.prog = fields[8]
        self.rwd = fields[9]
        self.line = fields[-1]
        self.pa_set = set()

    def __repr__(self):
        return '[{0}] {1}'.format(self.rwd, self.line)

    def add(self, pa):
        """Add a PathAction object to the involved set."""
        self.pa_set.add(pa)

    def paths(self):
        """Return the set of paths involved in PathActions."""
        return set([pa.path for pa in self.pa_set])

    def get_prereqs(self):
        """Return the list of prereq PathActions."""
        prqs = set([pa for pa in self.pa_set if pa.is_prereq()])
        return sorted(prqs, key=lambda p: p.path)

    def get_targets(self):
        """Return the list of target PathActions."""
        tgts = set([pa for pa in self.pa_set if pa.is_target()])
        return sorted(tgts)

# vim: ts=8:sw=4:tw=80:et:
