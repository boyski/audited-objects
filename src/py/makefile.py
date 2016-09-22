"""Classes to support makefile abstractions and creation."""

#############################################################################
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#############################################################################

from __future__ import print_function
from __future__ import unicode_literals

import os
import re

import childx  # pylint: disable=relative-import

BROKEN_SH = os.path.isfile('/bin/dash') and os.path.isfile('/bin/sh') and \
    os.stat('/bin/dash')[1] == os.stat('/bin/sh')[1]

BS = '\\'
COLON = ':'
INDENT = '  '
NL = '\n'
TAB = '\t'
TABNL = NL + TAB
SEP = '/'
SP = ' '
CONT = SP + BS + NL

# A bunch of special cases, mostly generated values which change
# unreliably. Also some that are just not of much use.
UNINTERESTING_VARS = set([
    'AS',
    'CO',
    'COFLAGS',
    'CTANGLE',
    'CURDIR',
    'CWEAVE',
    'F77',
    'F77FLAGS',
    'GET',
    'M2C',
    'MAKEFILES',
    'MAKEFILE_LIST',
    'MAKEFLAGS',
    'MAKE_VERSION',
    'OBJC',
    'OUTPUT_OPTION',
    'PC',
    'SUFFIXES',
    'TANGLE',
    'WEAVE',
])


def shellsafe(text):
    """True if text can be safely exposed to a POSIX shell."""
    # The RE here selects inert characters. Anything else should be quoted.
    # Special case - a tilde is inert except when leading.
    if text[0] != '~' and re.match(r'^[-+~\w/.%@,:{}=]+$', text):
        return True
    else:
        #  Another special case - a leading caret does not require quoting.
        return text[0] == '^' and '^' not in text[1:]


def quote(stuff):
    """Quote the provided list or string against shell expansion."""
    q1 = "'"
    q2 = '"'

    listin = isinstance(stuff, list)
    items = (stuff if listin else [stuff])
    result = []

    for item in items:
        if isinstance(item, list):
            result.append(' '.join(quote(item)))
            continue

        # In case we were passed an integral value.
        item = '%s' % item

        if shellsafe(item):
            result.append(item)
        elif any(c in '&|;' for c in item):
            # If these are present they're probably intended for the shell.
            result.append(item)
        elif q1 in item:
            result.append(q2 + item + q2)
        else:
            # Cosmetic: quote assignments as foo='bar', not 'foo=bar'.
            match = re.match(r'^(\w+=)(\S+.*)', item)
            if match:
                result.append(match.group(1) + q1 + match.group(2) + q1)
            else:
                result.append(q1 + item + q1)

    return result if listin else result[0]


def varify(text, makevars):
    """
    Substitute provided make variables into the text.

    Takes a string and tries to substitute in make variables such that
    '-c -g -O2 -Wall -DNDEBUG' might become '-c $(CFLAGS) $(DFLAGS)'.
    """
    text = '$$'.join(text.split('$'))
    for makevar in makevars:
        if makevar.val in text:
            text = makevar.expand().join(text.split(makevar.val))


def desh(line):
    """Turn "/bin/sh -c <cmd>" into "<cmd>" (without the quotes)."""
    # If the matched command was '/bin/sh -c "stuff"', what we want to
    # put in the makefile is just "stuff" (without the quotes) because
    # when make sees this it will run it as '/bin/sh -c "stuff"' again.
    # The simplest way to drop a shell level is to actually run the
    # shell in verbose (-v) and no-execute (-n) mode and capture the
    # output. BTW it turns out that due to subtle variations and bugs
    # in different Unix shells (bash, ksh, zsh, etc) it's better to
    # always use /bin/sh for this purpose. Usually faster too. But
    # there's an exception: some newer Linux distros use 'dash' as
    # their /bin/sh and dash doesn't seem to implement -n or -v as
    # expected so we have to work around that. The current hack is
    # that if /bin/sh == dash, use bash instead with some special flags.
    # Argh.
    parts = line.split(None, 2)
    if len(parts) == 3 and parts[0].endswith('sh') and \
       parts[1][0] == '-' and parts[1][-1] == 'c':
        flags, text = parts[1], parts[2]
        if BROKEN_SH:
            cmd = ['/bin/bash', '--norc', '--noprofile', '-nv', flags, text]
        else:
            cmd = ['/bin/sh', '-nv', flags, text]
        line = childx.check_output(cmd, stderr=childx.STDOUT).rstrip()
    return line


class Path(object):

    """A path which may be used in the makefile by a parameterized name."""

    def __init__(self, path, begin=None, macro=None):
        self.path = path
        self.ppath = None
        if begin and self.path.startswith(begin):
            self.ppath = quote(path.replace(begin, '$({})'.format(macro), 1))


class Variable(object):

    """Represent a make variable."""

    def __init__(self, line=None, name=None, eq=':=', val=None):
        self.explicit = True
        self.gnu = True
        self.eq = eq
        self.name, self.val = None, None
        if line:
            match = re.match(r'^(.*?)\s+([^=]*=)\s*(.*?)\s*$', line)
            if match:
                self.name, self.eq, self.val = match.groups()
        else:
            self.name, self.eq, self.val = name, eq, val

    def __repr__(self):
        result = '{} {}'.format(self.name, self.eq)
        if isinstance(self.val, basestring):
            result += SP + self.val
        else:
            btwn = CONT + INDENT
            result += btwn + btwn.join(self.val) + NL
        return result

    def assign(self, name, eq, val):
        """Assign a value to this variable."""
        self.name, self.eq, self.val = name, eq, val
        return self

    def expand(self):
        """Return the variable name in expandable make syntax."""
        return '${{{}}}'.format(self.name)

    def format(self):
        """Dump this variable in make syntax.."""
        return repr(self)


class Phony(object):

    """Represent a classic GNU make .PHONY target."""

    def __init__(self, tgt, prqs=None, recipe=None):
        self.tgt = tgt
        self.prqs = prqs
        self.recipe = recipe

    def __repr__(self):
        result = '.PHONY: {0}\n{0}:'.format(self.tgt)
        if self.prqs:
            result += SP + SP.join(self.prqs)
        if self.recipe:
            result += NL + TAB + self.recipe + NL
        return result + NL


class Rule(object):

    """Represent a classic make targets/prereqs/recipe triple."""

    def __init__(self, targets, prereqs=None, recipe=None):
        if isinstance(targets, basestring):
            self.targets = [targets]
        else:
            self.targets = targets
        self.prereqs = prereqs
        self.basevar = None

        if recipe:
            # In order to fit make recipes into a CSV format AO replaces
            # interior newlines with a token. We need to convert those
            # tokens to newline+tab for reasons of make syntax.
            recipe = TABNL.join(recipe.strip().split('^J'))
            recipe = desh(recipe)
        self.recipe = recipe

    def __repr__(self):
        pfx = self.basevar.expand() + SEP if self.basevar else None

        def prqstr():
            """Return a makefile-syntax prereq string."""
            prqs = ''
            end = len(self.prereqs) - 1
            for i, prereq in enumerate(reversed(self.prereqs)):
                prqs += INDENT
                prqs += os.path.join(pfx, prereq.path)
                if i < end:
                    prqs += CONT
                else:
                    prqs += NL
            return prqs

        result = ''
        end = len(self.targets) - 1
        if self.recipe:
            for i, target in enumerate(self.targets):
                result += os.path.join(pfx, target.path)
                result += COLON + CONT
                result += prqstr()
                result += TAB + self.recipe + NL
        else:
            for i, target in enumerate(self.targets):
                result += os.path.join(pfx, target.path)
                if i < end:
                    result += CONT + (INDENT * 2)
                else:
                    result += COLON
            result += CONT + prqstr()
        return result


class Makefile(object):

    """Manage a makefile abstraction and dump it in file form."""

    def __init__(self, basedir, srcbase):
        self.basevar = Variable(name=srcbase, val=basedir)
        self.items = []

    def __repr__(self):
        result = ''
        if self.basevar:
            result += repr(self.basevar)
            result += NL * 2
        for item in self.items:
            if isinstance(item, basestring):
                result += item
            else:
                if isinstance(item, Rule):
                    item.basevar = self.basevar
                result += repr(item)
            result += NL
        result = result.replace('\n\n\n', '\n\n')
        result = result.rstrip() + NL
        return result

    def append(self, item):
        """Add a new item to the end of the makefile object."""
        self.items.append(item)

# vim: ts=8:sw=4:tw=80:et:
