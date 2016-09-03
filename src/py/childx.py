"""
Wrapper over subprocess with xtrace verbosity added.

This adds a capability similar to the /bin/sh -x option to the
subprocess module.  With verbosity enabled, all command execs will
print "+ <cmdline>" to stderr. In all other respects it behaves just
like subprocess.

"""

import subprocess
import sys

# Pass these through so users don't need to import subprocess.
PIPE = subprocess.PIPE
STDOUT = subprocess.STDOUT

# Controls xtrace verbosity.
XTRACE = False


def set_xtrace(state):
    """Turn exec verbosity on or off."""
    global XTRACE  # pylint: disable=global-statement
    XTRACE = state


def xtrace(cmd, **kwargs):
    """Print the command to stderr."""
    # Allow this custom keyword to suppress verbosity for special cases.
    if not kwargs.pop('xtrace', True):
        return
    # Skip xtrace verbosity if stderr is redirected.
    if not XTRACE or kwargs.get('stderr') is not None:
        return
    if isinstance(cmd, basestring):
        sys.stderr.write('+ ' + cmd + '\n')
    else:
        sys.stderr.write('+ ' + ' '.join(cmd) + '\n')


def call(*popenargs, **kwargs):
    """Wrapper over subprocess function with xtrace verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.call(*popenargs, **kwargs)


def check_call(*popenargs, **kwargs):
    """Wrapper over subprocess function with xtrace verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.check_call(*popenargs, **kwargs)


def check_output(*popenargs, **kwargs):
    """Wrapper over subprocess function with xtrace verbosity added."""
    xtrace(popenargs[0], **kwargs)
    return subprocess.check_output(*popenargs, **kwargs)


class Popen(subprocess.Popen):
    """Wrapper over subprocess Popen class with xtrace verbosity added."""

    def __init__(self, cmd, **kwargs):
        xtrace(cmd, **kwargs)
        super(Popen, self).__init__(cmd, **kwargs)


class CalledProcessError(subprocess.CalledProcessError):
    """Wrapper over subprocess.CalledProcessError class."""
    pass

# vim: ts=8:sw=4:tw=80:et:
