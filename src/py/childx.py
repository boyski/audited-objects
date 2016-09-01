"""Wrapper over subprocess with verbosity added."""

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

import subprocess
import sys

PIPE = subprocess.PIPE
STDOUT = subprocess.STDOUT
VERBOSE = False


def set_verbosity(state):
    """Turn exec verbosity on or off."""
    global VERBOSE  # pylint: disable=global-statement
    VERBOSE = state


def xtrace(cmd, **kwargs):
    """Print the command to stderr."""
    # Custom keyword xtrace=False to suppress verbosity for special cases.
    if not kwargs.pop('xtrace', True):
        return
    if VERBOSE and kwargs.get('stderr') != STDOUT:
        if isinstance(cmd, basestring):
            sys.stderr.write('+ ' + cmd + '\n')
        else:
            sys.stderr.write('+ ' + ' '.join(cmd) + '\n')


def call(*popenargs, **kwargs):
    """Wrapper over subprocess function with verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.call(*popenargs, **kwargs)


def check_call(*popenargs, **kwargs):
    """Wrapper over subprocess function with verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.check_call(*popenargs, **kwargs)


def check_output(*popenargs, **kwargs):
    """Wrapper over subprocess function with verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.check_output(*popenargs, **kwargs)


class Popen(subprocess.Popen):
    """Wrapper over subprocess Popen class with verbosity added."""

    def __init__(self, cmd, **kwargs):
        xtrace(cmd, **kwargs)
        super(Popen, self).__init__(cmd, **kwargs)


class CalledProcessError(subprocess.CalledProcessError):
    """Wrapper over subprocess.CalledProcessError class."""
    pass

# vim: ts=8:sw=4:tw=80:et:
