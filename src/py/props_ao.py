"""Support for properties files."""

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

import collections
import copy
import json
import os
import os.path as op
import site

# Update sys.path to find required library modules.
site.addsitedir(op.dirname(op.realpath(__file__)))


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


def dict_merge(a, b):
    """
    Recursively combine two data structures comprised of dicts.

    In case of collision, "a" wins and a warning is optionally given.
    Lists are 'merged' by turning them into sets, combining
    the sets, and sorting the result back into a list.
    """
    if not isinstance(b, dict):
        return b
    for key in b:
        if key in a and isinstance(a[key], collections.abc.Mapping):
            a[key] = dict_merge(a[key], b[key])
        elif key in a and (isinstance(a[key], list) and
                           isinstance(b[key], list)):
            if not b[key]:
                pass
            elif not a[key]:
                a[key] = b[key]
            elif a[key] != b[key]:
                a[key] += [k for k in b[key] if k not in a[key]]
        else:
            if a.get(key) is None:
                a[key] = copy.deepcopy(b[key])

    return a


class Properties():
    """Parse AO properties files and allow lookups."""

    def __init__(self, name='ao_properties.json'):
        self.name = name
        self.cfg = {}

        # Highly unlikely to exist but for completeness ...
        etc_props = op.join('/etc', name)
        if op.isfile(etc_props):
            props = json.load(etc_props)
            dict_merge(self.cfg, props)

        # Look for a properties file at the base of the project.
        proj_props = find_file_up(op.join('.ao', name))
        if proj_props:
            props = json.load(proj_props)
            dict_merge(self.cfg, props)

        # Let exported properties win.
        pfxup = 'AO_'
        props = {}
        for ev, value in os.environ.items():
            if ev.startswith((pfxup, '_' + pfxup)):
                name = ev.split(pfxup, 1)[1]
                props[name] = value
        if props:
            dict_merge(self.cfg, props)

        # Keep keys sorted alphabetically. No particular reason.
        self.cfg = {key: self.cfg[key] for key in sorted(self.cfg)}

    def get(self, prop, default=None):
        """Return the value of the named property."""
        return self.cfg.get(prop, default)


# vim: sw=4:et:tw=79:cc=+1
