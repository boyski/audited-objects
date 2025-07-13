#!/usr/bin/env python3
"""
TBD.
"""

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

import json

import actions_ao

# There's another copy in C.
SEP = '=-=-=-=-=-=-='


def is_vcs(path):
    """Return True if the path looks like version-control metadata."""

    return '.git' in path or '.svn' in path


def parse_csv(data):
    """Parse old-style CSV data into a list of CommandActions."""

    ca, calist = None, []
    for line in data.splitlines():
        if not line:
            ca = None
        elif line[0].isdigit():
            if not ca:
                ca = actions_ao.CommandAction(line)
                calist.append(ca)
        elif line[0].isupper():
            pa = actions_ao.PathAction(line)
            if is_vcs(pa.path) or pa.is_unlink:
                continue
            ca.add_pa(pa)
        else:
            ca = None

    return calist


def parse_json(data):
    """Parse JSON data into a list of CommandActions."""

    calist = []
    for para in data.split(SEP):
        if len(para) < 2:
            continue
        doc = json.loads(para)
        ca = actions_ao.CommandAction(doc['CommandActions'][0])
        for pa in doc['PathActions']:
            ca.add_pa(actions_ao.PathAction(pa))
        calist.append(ca)
    return calist


def parse(infiles):
    """Parse input data into a list of CommandActions."""

    calist = []
    for path in infiles:
        if path == '-':
            path = '/dev/stdin'
        with open(path, encoding='utf-8') as f:
            contents = f.read()

        if contents[0].isalnum():
            calist += parse_csv(contents)
        else:
            calist += parse_json(contents)
    return calist

# vim: sw=4:et:tw=79:cc=+1
