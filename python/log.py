# Copyright 2024, David Goodwin. All rights reserved.

import sys


# CifreLoggerBase
#
# Base class for logging and related utilities
#
class CifreLoggerBase:
    def __init__(self, indent=0, verbose=False, quiet=False, debug=False):
        self._verbose = verbose
        self._quiet = quiet
        self._indent = indent
        self._debug = debug

    # Create a new logger and initialize its settings from an existing
    # logger. Optionally allow existing logger settings to be
    # overridden.
    @classmethod
    def copy(cls, other, indent=None, verbose=None, quiet=None, debug=None):
        return cls(indent=other._indent if indent is None else indent,
                   verbose=other._verbose if verbose is None else verbose,
                   quiet=other._quiet if quiet is None else quiet,
                   debug=other._debug if debug is None else debug)

    def verbose(self):
        return self._verbose
    def quiet(self):
        return self._quiet
    def debug(self):
        return self._debug
    def indent(self):
        return self._indent

    def indent_multiline(self, msg, indent):
        # 'msg' may be multiline, so we must split it into lines
        # before inserting extra indent.
        if indent > 0:
            lines = msg.splitlines()
            lines = ['{}{}'.format('\t' * indent, l) for l in lines]
            msg = '\n'.join(lines)
        return msg

    def first_line(self, msg, indent=0):
        # Return first line of 'msg'.
        msg = msg.splitlines()[0]
        if indent > 0:
            msg = ['{}{}'.format('\t' * indent, msg)]
        return msg

    def log(self, msg, extra_indent=0, force=False):
        if force or not self._quiet:
            msg = self.indent_multiline(msg, self._indent + extra_indent)
            try:
                self._log(msg)
            except Exception as e:
                self.fail(f'<failed to log>: {e}')

    def log_verbose(self, msg, extra_indent=0):
        if self._verbose:
            self.log(msg, extra_indent=extra_indent, force=True)

    def log_debug(self, msg, extra_indent=0):
        if self._debug:
            self.log(msg, extra_indent=extra_indent, force=True)

    def fail(self, msg):
        self.fail_if(True, msg)

    def fail_if(self, p, msg):
        if __debug__:
            assert not p, 'error: {}'.format(msg)
        elif p:
            print('error: {}'.format(msg), file=sys.stderr)
            sys.exit(1)


# CifreLogger
#
# Log to a file or stdout/stderr
#
class CifreLogger(CifreLoggerBase):
    def __init__(self, log_file=sys.stdout, indent=0, verbose=False, quiet=False, debug=False):
        CifreLoggerBase.__init__(self, indent, verbose, quiet, debug)
        self._log_file = log_file

    def _log(self, msg):
        print(msg, file=self._log_file)


# CifreLoggerString
#
# Log to a string.
#
class CifreLoggerString(CifreLoggerBase):
    def __init__(self, indent=0, verbose=False, quiet=False, debug=False):
        CifreLoggerBase.__init__(self, indent, verbose, quiet, debug)
        self._lines = []

    def _log(self, msg):
        self._lines.append(msg)

    def __str__(self):
        return '\n'.join(self._lines)


DEFAULT_LOGGER = CifreLogger()
DEFAULT_STRING_LOGGER = CifreLoggerString()
