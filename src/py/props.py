"""Support for .properties files."""

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

import configparser
import os
import os.path as op
import site

# Update sys.path to find required library modules.
site.addsitedir(op.dirname(op.realpath(__file__)))

PFX = 'AO_'


def find_file_up(wanted, start_dir='.'):
    """Search upward from start_dir toward the root for a named file."""

    curr_dir = op.abspath(start_dir)
    while True:
        candidate = op.join(curr_dir, wanted)
        if op.exists(candidate):
            return candidate
        parent = op.dirname(curr_dir)
        if parent == curr_dir:
            break
        curr_dir = parent
    return None


class Properties():
    """Parse AO .properties files and allow lookups."""

    def __init__(self):
        self.cfg = configparser.ConfigParser()

        # Highly unlikely but for completeness ...
        if op.isfile('/etc/ao.properties'):
            self.parse('/etc/ao.properties')

        # Look for a properties file at the base of the project.
        proj_props = find_file_up(op.join('.ao', 'ao.properties'))
        if proj_props:
            self.parse(proj_props)

        # Let exported properties win.
        for ev, value in os.environ.items():
            if ev.startswith((PFX, '_' + PFX)):
                name = ev.split(PFX, 1)[1]
                self.cfg['UNNAMED'][name] = value

    def parse(self, path):
        """Parse the provided properties file."""
        with open(path, encoding='utf-8') as f:
            self.cfg.read_string('[UNNAMED]\n' + f.read())

    def __getattr__(self, attr):
        return self.cfg['UNNAMED'].get(attr)


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# fill-column: 79
# End:
#
# vim: sw=4:et:tw=79:cc=+1
