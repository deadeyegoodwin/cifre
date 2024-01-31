#!/usr/bin/env python3

# Copyright 2024, David Goodwin. All rights reserved.

import argparse
import os
import sys
import time

from log import CifreLogger

FLAGS = None
LOG = None

def main():
    global FLAGS
    global LOG

    parser = argparse.ArgumentParser()
    group_qv = parser.add_mutually_exclusive_group()
    group_qv.add_argument(
        '-q',
        '--quiet',
        action='store_true',
        required=False,
        help='Disable console output.')
    group_qv.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        required=False,
        help='Enable verbose output.')
    parser.add_argument(
        '--log-id',
        action='store_true',
        required=False,
        help='Enable object IDs in log output.')
    parser.add_argument(
        '--log-debug',
        action='store_true',
        required=False,
        help='Enable debug logging.')

    FLAGS = parser.parse_args()
    LOG = CifreLogger(verbose=FLAGS.verbose, quiet=FLAGS.quiet,
                      show_id=FLAGS.log_id, debug=FLAGS.log_debug)

    if LOG.verbose():
        LOG.log_verbose('Settings:')
        for f in vars(FLAGS):
            LOG.log_verbose(f'\t{f}: {vars(FLAGS)[f]}')


if __name__ == '__main__':
    main()
