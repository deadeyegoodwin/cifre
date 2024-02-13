#!/usr/bin/env python3

# Copyright 2024, David Goodwin. All rights reserved.

import argparse
import os
import shutil
import subprocess
import sys
sys.path.append('./nnet')

from log import CifreLogger
from nnet import CifreNNetManager
from basic_nnet import CifreBasicNNet

FLAGS = None
LOG = None

def parse_workdir(arg):
    """
    Parse 'arg'ument as a workdir path.
    """
    if os.path.exists(arg) and not os.path.isdir(arg):
        raise FileExistsError(f"{arg} exists and is not a directory")
    return arg

def parse_shape(arg):
    """
    Parse 'arg'ument as a shape in format 'x, y, z, ...' and return
    tuple representation.
    """
    arg = arg.strip()
    if len(arg) > 0:
        return tuple(int(x) for x in arg.split(','))
    raise Exception(f"{arg} must describe a shape as 'a,b,...'")

def argify_shape(shape):
    """
    Convert 'shape' tuple into equivalent argument string.
    """
    return ','.join(map(str, shape))

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

    parser.add_argument(
        '--game',
        type=str,
        default="celcath",
        required=False,
        help='The game, default is celcath.')
    parser.add_argument(
        '--game-shape',
        type=parse_shape,
        required=True,
        help='Game shape, for example --game-shape=8,8.')

    parser.add_argument(
        '--workdir',
        type=parse_workdir,
        default='/tmp/cifre',
        required=False,
        help='Working directory, default is /tmp/cifre.')
    parser.add_argument(
        '--reset-workdir',
        action='store_true',
        required=False,
        help='Create workdir if it does not exist, clear workdir if it does exist.')

    FLAGS = parser.parse_args()
    LOG = CifreLogger(verbose=FLAGS.verbose, quiet=FLAGS.quiet,
                      show_id=FLAGS.log_id, debug=FLAGS.log_debug)

    if LOG.verbose():
        LOG.log_verbose('Settings:')
        for f in vars(FLAGS):
            LOG.log_verbose(f'\t{f}: {vars(FLAGS)[f]}')

    # Create / clear workdir.
    if FLAGS.reset_workdir:
        if os.path.isdir(FLAGS.workdir):
            shutil.rmtree(FLAGS.workdir)
        if os.path.exists(FLAGS.workdir):
            raise FileExistsError(f"{FLAGS.workdir} exists and is not a directory")
        os.mkdir(FLAGS.workdir)

    # For now there is only 1 game recognized so just hack it in here.
    if FLAGS.game != 'celcath':
        raise ValueError(f"unexpected --game={FLAGS.game}, options are 'celcath'")
    game_action_count = FLAGS.game_shape[0] * FLAGS.game_shape[1] + 1

    # Use a simple model...
    nnet_args = {
        'num_rows' : FLAGS.game_shape[0],
        'num_cols' : FLAGS.game_shape[1],
        'num_actions' : game_action_count,
        'dropout': 0.25,
        'num_channels': 512 }
    nnet = CifreBasicNNet(nnet_args)

    # Create the manager for the nnet, and write the initial model
    # into FLAGS.workdir/models
    manager = CifreNNetManager(nnet, FLAGS.game_shape, FLAGS.workdir)
    manager.save_triton_repository('initial')

    # Launch cifre executable to perform MCTS using the just trained
    # model.
    cifre_mcts_cmd = [
        '../build/cifre',
        '--log-high' if FLAGS.verbose else '',
        '--workdir', FLAGS.workdir,
        '--model', 'initial',
        '--game', FLAGS.game,
        '--game-shape', argify_shape(FLAGS.game_shape) ]
    process = subprocess.Popen(cifre_mcts_cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while process.poll() is None:
        print(process.stdout.readline().rstrip())
    print(process.stdout.read())
    process.stdout.close()
    print(f"Return code is {process.poll()}")

    sys.exit(0)

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
