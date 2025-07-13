"""Classes to work with raw audit data from libAO.so."""

#############################################################################
# Copyright (c) 2005-2025 David Boyce.  All rights reserved.
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

import os.path as op
import site

# Update sys.path to find required library modules.
site.addsitedir(op.dirname(op.realpath(__file__)))

CA_SPLITS = 13
PA_SPLITS = 16
SEP = ','


class PathAction():
    """Parse PA data into its component parts."""

    def __init__(self, desc):
        if isinstance(desc, str):
            fields = desc.rstrip().split(SEP, PA_SPLITS)
            self.dcode = fields[14]
            self.op = fields[0]
            self.path2 = fields[-2]
            self.path = fields[-1]
        else:
            self.call = desc['CALL']
            self.ccode = desc['CCODE']
            self.datatype = desc['DATATYPE']
            self.dcode = desc['DCODE']
            self.depth = desc['DEPTH']
            self.fsname = desc['FSNAME']
            self.mode = desc['MODE']
            self.mtime = desc['MTIME']
            self.op = desc['OP']
            self.path = desc['PATH']
            self.path2 = desc['PATH2']
            self.pccode = desc['PCCODE']
            self.pid = desc['PID']
            self.ppid = desc['PPID']
            self.size = desc['SIZE']
            self.tid = desc['TID']
            self.timestamp = desc['TIMESTAMP']
        self.is_member = not op.isabs(self.path)

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

        # The next-to-last (path2) slot is reserved for a symlink target.
        if self.path2:
            if not op.isabs(self.path2):
                self.path2 = op.join(op.dirname(self.path), self.path2)
            self.path2 = op.normpath(self.path2)

    def __lt__(self, other):
        """

        Sometimes it's helpful to treat one among multiple sibling
        targets as "primary". There's some black magic to deciding which
        one that should be. First we have some special cases: directories
        are pushed to the back of the pack as are files whose name
        starts with ".". Members are pushed to the front of the pack.
        Files newly created are preferred over files merely appended to.
        Next we observe that generated "frill" files (.pdb and so on)
        tend to be distinguished by additional extensions or placement
        in subdirectories, so we favor shorter paths.  The first target
        in the list after all this is considered primary.

        This is just a heuristic and subject to breakage and change.
        Always looking for a better way.

        Similar logic is available for sorting any PA set but is important
        primarily for targets.
        """
        if self.is_member != other.is_member:
            return not bool(self.is_member)
        if self.path[0] == '.' and other.path[0] != '.':
            return True
        if self.is_dir and not other.is_dir:
            return True
        if self.is_append and not other.is_append:
            return True
        diff = len(self.path) - len(other.path)
        if diff == 0:
            return self.path < other.path
        return diff < 0

    def __eq__(self, other):
        return self.path == other.path and self.op == other.op

    def __hash__(self):
        return hash(self.op + self.path)

    def __repr__(self):
        return ' '.join([self.op, self.path])

    def exists(self):
        """Report whether this path currently exists."""
        return op.exists(self.path)

    @property
    def is_prereq(self):
        """Report whether this path action represents a prereq (read)."""
        return self.op in 'RX'

    @property
    def is_target(self):
        """Report whether this path action represents a target (write)."""
        return self.op not in 'RXU'

    @property
    def is_append(self):
        """Report whether this path action represents a file append (write)."""
        return self.op == 'A'

    @property
    def is_symlink(self):
        """Report whether this path action represents a symlink creation."""
        return self.op == 'S'

    @property
    def is_unlink(self):
        """Report whether this path action represents a file removal."""
        return self.op == 'U'

    @property
    def is_dir(self):
        """Report whether this path action represents a mkdir."""
        return self.op == 'D'


class CommandAction():
    """Parse CA data into its component parts."""

    def __init__(self, desc):
        if isinstance(desc, str):
            fields = desc.rstrip().split(SEP, CA_SPLITS)
            self.cmdline = fields[-1]
            self.prog = fields[8]
            self.rwd = fields[9]
        else:
            self.cmdline = desc['CMDLINE']
            self.depth = desc['DEPTH']
            self.duration = desc['DURATION']
            self.freetext = desc['FREETEXT']
            self.host = desc['HOST']
            self.pathcode = desc['PATHCODE']
            self.ccode = desc['CCODE']
            self.pccode = desc['PCCODE']
            self.pid = desc['PID']
            self.ppid = desc['PPID']
            self.prog = desc['PROG']
            self.recycled = desc['RECYCLED']
            self.rwd = desc['RWD']
            self.started = desc['STARTED']
        self.pa_set = set()

    def __repr__(self):
        return f'[{self.rwd}] {self.cmdline}'

    def add_pa(self, pa):
        """Add a PathAction object to the involved set."""
        self.pa_set.add(pa)

    def paths(self):
        """Return the list of paths involved in PathActions."""
        return sorted({pa.path for pa in self.pa_set})

    def prereqs(self):
        """Return the list of prereq PathActions."""
        return sorted([pa for pa in self.pa_set if pa.is_prereq])

    def targets(self):
        """Return the list of target PathActions."""
        return sorted([pa for pa in self.pa_set if pa.is_target])

# vim: sw=4:et:tw=79:cc=+1
