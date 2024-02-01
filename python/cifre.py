#!/usr/bin/env python3

# Copyright 2024, David Goodwin. All rights reserved.

import argparse
import sys
sys.path.append('./nnet')

from log import CifreLogger
from nnet import CifreNNetManager
from basic_nnet import CifreBasicNNet

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

    nnet_args = {
        'num_rows' : 8,  #FIXME
        'num_cols' : 8,  #FIXME
        'num_actions' : 65,  #FIXME
        'dropout': 0.25,
        'num_channels': 512 }
    nnet = CifreBasicNNet(nnet_args)

    manager = CifreNNetManager(nnet)

    ## FIXME
    import numpy as np
    dataset = []
    for _ in range(2):
        b = np.random.random((nnet_args['num_rows'], nnet_args['num_cols'])).astype(np.float32) # FIXME
        p = np.random.random(nnet_args['num_actions']).astype(np.float32)
        v = 1
        dataset.append((b, p, v))

    train_args = {
        'num_epochs': 4,
        'batch_size': 2 }
    manager.train(train_args, dataset)
    manager.save_checkpoint('/tmp/cpts', 'cpt.pth.tar', create_folder=True, overwrite=True)
    manager.load_checkpoint('/tmp/cpts', 'cpt.pth.tar')
    manager.train(train_args, dataset)


if __name__ == '__main__':
    main()
