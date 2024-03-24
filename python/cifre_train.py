#!/usr/bin/env python3

# Copyright 2024, David Goodwin. All rights reserved.

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
sys.path.append('./nnet')

from log import CifreLogger
from dataset import CifreDataset
from nnet import CifreNNetManager
from basic_conv_nnet import CifreBasicConvNNet
from basic_fc_nnet import CifreBasicFcNNet

FLAGS = None
LOG = None

def parse_existing_dir(arg):
    """
    Parse 'arg'ument as a directory path that must already exist.
    """
    if not os.path.exists(arg):
        raise FileNotFoundError(f"{arg} does not exist")
    elif not os.path.isdir(arg):
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

def parse_nonnegative_int(value):
    ival = int(value)
    if ival < 0:
        raise Exception(f"non-negative integer expected, got {ival}")
    return ival

def parse_positive_int(value):
    ival = int(value)
    if ival <= 0:
        raise Exception(f"positive integer expected, got {ival}")
    return ival

def parse_positive_float(value):
    fval = float(value)
    if fval <= 0.0:
        raise Exception(f"positive float expected, got {fval}")
    return fval

def argify_shape(shape, delimiter):
    """
    Convert 'shape' tuple into equivalent argument string.
    """
    return delimiter.join(map(str, shape))

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
        default="tictactoe",
        required=False,
        help='The game, default is tictactoe.')
    parser.add_argument(
        '--game-shape',
        type=parse_shape,
        required=True,
        help='Game shape, for example --game-shape=8,8.')

    parser.add_argument(
        '--iteration-count',
        type=parse_positive_int,
        default=10,
        required=False,
        help=('Number of iterations to run for training. Each iteration plays ' +
              '--game-count games using AlphaZero to collect training samples. ' +
              'The training samples are then used to train a new nnet for the next ' +
              'iteration.'))
    parser.add_argument(
        '--game-count',
        type=parse_positive_int,
        default=10,
        required=False,
        help=('Number of games to play using AlphaZero for each of the --iteration-count ' +
              'iterations.'))
    parser.add_argument(
        '--max-turns-per-game',
        type=parse_positive_int,
        default=50,
        required=False,
        help=('Maximum number of turns allowed per game. One turn consists of each ' +
              'player getting a chance to take actions.'))
    parser.add_argument(
        '--alphazero-update-count',
        type=parse_positive_int,
        default=10,
        required=False,
        help=('Number of updates/searches made to the AlphaZero tree before determining ' +
              'each game action.'))
    parser.add_argument(
        '--alphazero-cpuct',
        type=parse_positive_float,
        default=4.0,
        required=False,
        help=('AlphaZero cpuct value.'))
    parser.add_argument(
        '--epoch-count',
        type=parse_positive_int,
        default=10,
        required=False,
        help=('Number of training epochs to run. Default is 10.'))
    parser.add_argument(
        '--batch_size',
        type=parse_positive_int,
        default=8,
        required=False,
        help=('Batch size to use when training nnet. Default is 8.'))

    parser.add_argument(
        '--repo-dir',
        type=parse_existing_dir,
        required=True,
        help='Model repository directory.')
    parser.add_argument(
        '--train-dir',
        type=parse_existing_dir,
        required=True,
        help='Training data directory.')

    FLAGS = parser.parse_args()
    LOG = CifreLogger(verbose=FLAGS.verbose, quiet=FLAGS.quiet,
                      show_id=FLAGS.log_id, debug=FLAGS.log_debug)

    if LOG.verbose():
        LOG.log_verbose('cifre_train.py')
        LOG.log_verbose('Settings:')
        for f in vars(FLAGS):
            LOG.log_verbose(f'\t{f}: {vars(FLAGS)[f]}')

    # FIXME
    if FLAGS.game == 'tictactoe':
        nnet_args = {
            'num_rows' : FLAGS.game_shape[0],
            'num_cols' : FLAGS.game_shape[1],
            'num_actions' : FLAGS.game_shape[0] * FLAGS.game_shape[1],
            'dropout': 0.25,
            'num_channels': 512 }
        nnet = CifreBasicFcNNet(nnet_args)
    elif FLAGS.game == 'celcath':
        nnet_args = {
            'num_rows' : FLAGS.game_shape[0],
            'num_cols' : FLAGS.game_shape[1],
            'num_actions' : FLAGS.game_shape[0] * FLAGS.game_shape[1] + 2,
            'dropout': 0.25,
            'num_channels': 512 }
        nnet = CifreBasicConvNNet(nnet_args)
    else:
        raise ValueError(f"unexpected --game={FLAGS.game}, options are 'tictactoe', 'celcath'")

    # The same nnet is used for all iterations.
    manager = CifreNNetManager(nnet, FLAGS.game_shape, FLAGS.repo_dir)

    # Keep track of the trained model that gives the best
    # results. After each iteration, the just trained model competes
    # against the current best to see if it is better.
    best_model_name = None

    for iter in range(FLAGS.iteration_count):
        model_name = f'iter{iter}'
        manager.save_triton_repository(model_name)

        train_dir = pathlib.Path(FLAGS.train_dir) / model_name
        if os.path.exists(train_dir):
            shutil.rmtree(train_dir)
        os.makedirs(train_dir)

        # Launch cifre executable to collect training data.
        game_cmd = [
            f'../build/install/bin/{FLAGS.game}',
            '--verbose' if FLAGS.verbose else '',
            '--train-dir', train_dir,
            '--board-size', argify_shape(FLAGS.game_shape, 'x'),
#            f'--min-actions-per-turn={min_actions_per_turn}', FIXME
#            f'--max-actions-per-turn={max_actions_per_turn}',
#            f'--max-turns-per-game={FLAGS.max_turns_per_game}',
            f'--game-count={FLAGS.game_count}' ]

        game_cmd.extend(['--player1=alphazero', f'--name=p1iter{iter}', '--model', model_name,
                         f'--alphazero-update-count={FLAGS.alphazero_update_count}',
                         f'--alphazero-cpuct={FLAGS.alphazero_cpuct}' ])

        # First play against a random opponent to avoid the case where
        # initial alphazero model cannot complete any games.
        if iter == 0:
            game_cmd.extend(['--player2=random', '--name=random'])
        else:
            game_cmd.extend(['--player2=alphazero', f'--name=p2iter{iter}', '--model', model_name,
                             f'--alphazero-update-count={FLAGS.alphazero_update_count}',
                             f'--alphazero-cpuct={FLAGS.alphazero_cpuct}' ])

        LOG.log(f'\n==== Iteration {iter} ====')
        process = subprocess.Popen(game_cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        while process.poll() is None:
            print(process.stdout.readline(), end='')
        print(process.stdout.read(), end='')
        process.stdout.close()
        cret = process.poll()
        LOG.fail_if(cret != 0, f'cifre return code is {cret}')

        # Dataset
        dataset = CifreDataset(LOG, train_dir)

        train_args = {
            'num_epochs': FLAGS.epoch_count,
            'batch_size': FLAGS.batch_size }
        manager.train(train_args, dataset)
        manager.save_checkpoint(train_dir, 'cpt.pth.tar', create_folder=False, overwrite=True)

        # Compare 'model_name' against the best model.
        if best_model_name is None:
            best_model_name = model_name
        else:
            LOG.log(f'==== Compare {model_name} to {best_model_name} ====')
            game_count = winner1_count = winner2_count = draw_count = 0

            # Give each player a chance to go first...
            cmp_game_count = 10  # FIXME
            for starting_idx in [0, 1]:
                report_file = train_dir / f'cmp{starting_idx}.json'
                cmp_cmd = [
                    f'../build/install/bin/{FLAGS.game}',
                    '--verbose' if FLAGS.verbose else '--quiet',
                    '--board-size', argify_shape(FLAGS.game_shape, 'x'),
                    '--report-file', report_file,
                    f'--game-count={cmp_game_count}',
                    '--player1=alphazero', f'--name={model_name}', '--model', model_name,
                    '--starting' if starting_idx == 0 else '',
                    f'--alphazero-update-count={FLAGS.alphazero_update_count}',
                    '--player2=alphazero', f'--name={best_model_name}', '--model', best_model_name,
                    '--starting' if starting_idx == 1 else '',
                    f'--alphazero-update-count={FLAGS.alphazero_update_count}']

                process = subprocess.Popen(cmp_cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                while process.poll() is None:
                    print(process.stdout.readline(), end='')
                print(process.stdout.read(), end='')
                process.stdout.close()
                cret = process.poll()
                LOG.fail_if(cret != 0, f'cifre return code is {cret}')

                # Read and aggregate comparison results
                with report_file.open('r', encoding ='utf-8') as f:
                    j = json.load(f)
                    game_count += j['game-results']['count']
                    winner1_count += j['game-results']['player1']
                    winner2_count += j['game-results']['player2']
                    draw_count += j['game-results']['draw']

            LOG.log(f'games: {game_count}, {model_name} {winner1_count}, '
                    f'{best_model_name} {winner2_count}, draw {draw_count}')
            if winner1_count > winner2_count:
                LOG.log(f'best model now {model_name}')
                best_model_name = model_name
    LOG.log(f'==== Training complete, best model {best_model_name} ====')

if __name__ == '__main__':
    main()
