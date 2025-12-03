#!/usr/bin/env python3

# Copyright 2025, David Goodwin. All rights reserved.

import argparse
import json
import os
import pathlib
import multiprocessing
import shutil
import subprocess
import sys
sys.path.append('./nnet')

from log import CifreLogger
from dataset import CifreDataset
from nnet import CifreNNetManager
from basic_conv_nnet import CifreBasicConvNNet
from basic_fc_nnet import CifreBasicFcNNet
from residual_conv_nnet import CifreResidualConvNNet

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

def filter_specified(flag, value):
    return '' if (value is None) else f'{flag}={value}'

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
        '--debug',
        action='store_true',
        required=False,
        help='Enable debug output.')

    parser.add_argument(
        '--game',
        type=str,
        default="tictactoe",
        required=False,
        help='The game, default is tictactoe.')
    parser.add_argument(
        '--game-shape',
        type=parse_shape,
        required=False,
        help=('Game shape, for example --game-shape=8,8. By default game '
              'shape is determined by game selected with --game.'))

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
        default=1,
        required=False,
        help=('Number of games to play with AlphaZero self-play for each of the ' +
              '--iteration-count iterations. Training samples are collected from ' +
              'these games.'))
    parser.add_argument(
        '--game-parallel',
        type=parse_positive_int,
        required=False,
        help=('When playing the --game-count games, the number of games to play ' +
              'in parallel.'))
    parser.add_argument(
        '--max-turns-per-game',
        type=parse_positive_int,
        required=False,
        help=('Maximum number of turns allowed per game. One turn consists of each ' +
              'player getting a chance to take actions.'))
    parser.add_argument(
        '--alphazero-search-count',
        type=parse_positive_int,
        required=False,
        help=('Number of searches made to the AlphaZero tree when determining ' +
              'each game action during training.'))
    parser.add_argument(
        '--alphazero-search-depth',
        type=parse_positive_int,
        required=False,
        help=('Maximum depth of a search made to the AlphaZero tree when determining ' +
              'each game action during training.'))
    parser.add_argument(
        '--alphazero-cpuct',
        type=parse_positive_float,
        required=False,
        help=('AlphaZero cpuct value during training.'))
    parser.add_argument(
        '--epoch-count',
        type=parse_positive_int,
        default=16,
        required=False,
        help=('Number of training epochs to run. Default is 10.'))
    parser.add_argument(
        '--train-lr',
        type=parse_positive_float,
        required=False,
        default=0.001,
        help=('Learning rate to use when training nnet. Default 0.001.'))
    parser.add_argument(
        '--train-batch-size',
        type=parse_positive_int,
        required=False,
        help=('Batch size to use when training nnet. Default determined by nnet.'))
    parser.add_argument(
        '--infer-batch-size',
        type=parse_positive_int,
        required=False,
        help=('Batch size to use when running inference on nnet. Default determined by nnet.'))

    parser.add_argument(
        '--cmp-game-count',
        type=parse_positive_int,
        default=20,
        required=False,
        help=('Number of games to play when comparing two nnet models to decide which ' +
              'is better. Each player gets this number of games to start first, so the ' +
              'total number of games played is 2 * --cmp-game-count.'))

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
    LOG = CifreLogger(verbose=FLAGS.verbose, quiet=FLAGS.quiet, debug=FLAGS.debug)

    if LOG.verbose():
        LOG.log_verbose('cifre_train.py')
        LOG.log_verbose('Settings:')
        for f in vars(FLAGS):
            LOG.log_verbose(f'\t{f}: {vars(FLAGS)[f]}')

    # FIXME Should get default game shape, number actions from game
    # executable using --report-file.
    if FLAGS.game == 'tictactoe':
        game_shape = (3, 3) if (FLAGS.game_shape is None) else FLAGS.game_shape
        nnet_shape = game_shape
        train_batch_size = 2 if (FLAGS.train_batch_size is None) else FLAGS.train_batch_size
        infer_batch_size = 64 if (FLAGS.infer_batch_size is None) else FLAGS.infer_batch_size
        LOG.fail_if(len(game_shape) != 2, "tictactoe requires shape rank 2")
        nnet_args = {
            'batch_size' : train_batch_size,
            'num_rows' : game_shape[0],
            'num_cols' : game_shape[1],
            'num_actions' : game_shape[0] * game_shape[1],
            'dropout' : 0.25,
            'num_channels' : 512 }
        nnet = CifreBasicFcNNet(nnet_args)
    elif FLAGS.game == 'connect4':
        game_shape = (6, 7) if (FLAGS.game_shape is None) else FLAGS.game_shape
        nnet_shape = game_shape
        train_batch_size = 16 if (FLAGS.train_batch_size is None) else FLAGS.train_batch_size
        infer_batch_size = 32 if (FLAGS.infer_batch_size is None) else FLAGS.infer_batch_size
        LOG.fail_if(len(game_shape) != 2, "connect4 requires shape rank 2")
        nnet_args = {
            'batch_size' : train_batch_size,
            'num_rows' : game_shape[0],
            'num_cols' : game_shape[1],
            'num_actions' : game_shape[1],
            'dropout' : 0.25,
            'num_channels' : 512 }
        nnet = CifreBasicConvNNet(nnet_args)
    elif FLAGS.game == 'othello':
        game_shape = (8, 8) if (FLAGS.game_shape is None) else FLAGS.game_shape
        nnet_shape = game_shape
        train_batch_size = 16 if (FLAGS.train_batch_size is None) else FLAGS.train_batch_size
        infer_batch_size = 32 if (FLAGS.infer_batch_size is None) else FLAGS.infer_batch_size
        LOG.fail_if(len(game_shape) != 2, "othello requires shape rank 2")
        nnet_args = {
            'batch_size' : train_batch_size,
            'num_rows' : game_shape[0],
            'num_cols' : game_shape[1],
            'num_actions' : game_shape[0] * game_shape[1] + 1,
            'dropout' : 0.25,
            'num_channels' : 512 }
        nnet = CifreBasicConvNNet(nnet_args)
    elif FLAGS.game == 'gettysburg':
        # FIXME, game shape is fixed so just need to check/override here
        num_planes = 6  # see gettysburg::BoardCellFactory::PLANE_COUNT
        game_shape = (11, 21) if (FLAGS.game_shape is None) else FLAGS.game_shape
        nnet_shape = ( # see gettysburg::Board constructor
            ((game_shape[0] - 1) * 2) + 1, ((game_shape[1] - 1) * 2) + 1, num_planes)
        train_batch_size = 16 if (FLAGS.train_batch_size is None) else FLAGS.train_batch_size
        infer_batch_size = 32 if (FLAGS.infer_batch_size is None) else FLAGS.infer_batch_size
        LOG.fail_if(len(game_shape) != 2, "gettysburg requires shape rank 2")
        nnet_args = {
            'batch_size' : train_batch_size,
            'num_planes' : num_planes,
            'num_rows' : nnet_shape[0],
            'num_cols' : nnet_shape[1],
            'num_residual_blocks' : 2,
            'num_actions' : 7, # FIXME
            'num_channels' : 128 }
        nnet = CifreResidualConvNNet(nnet_args)
    else:
        raise ValueError(f"unexpected --game={FLAGS.game}, "
                         "options are 'tictactoe', 'othello', 'connect4', 'gettysburg'")

    # The same nnet is used for all iterations.
    manager = CifreNNetManager(LOG, nnet, nnet_shape, infer_batch_size, FLAGS.repo_dir)

    # Set parallelism for cifre game execution based on the number of cpu cores.
    game_parallel = (multiprocessing.cpu_count() * 2) if (FLAGS.game_parallel is None) else FLAGS.game_parallel

    # Keep track of the trained model that gives the best
    # results. After each iteration, the just trained model competes
    # against the current best to see if it is better.
    best_model_name = None
    best_model_cpt_dir = None

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
            '--parallel', str(game_parallel),
            '--train', '--train-dir', train_dir,
            '--game-shape', argify_shape(game_shape, ','),
            f'--game-count={FLAGS.game_count}',
            filter_specified('--max-turns-per-game', FLAGS.max_turns_per_game),
            '--player1=alphazero', '--alphazero-model', model_name,
            filter_specified('--alphazero-search-count', FLAGS.alphazero_search_count),
            filter_specified('--alphazero-search-depth', FLAGS.alphazero_search_depth),
            filter_specified('--alphazero-cpuct', FLAGS.alphazero_cpuct) ]

        LOG.log(f'\n==== Iteration {iter} ====')
        process = subprocess.Popen(game_cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        while process.poll() is None:
            print(process.stdout.readline(), end='', flush=True)
        print(process.stdout.read(), end='', flush=True)
        process.stdout.close()
        cret = process.poll()
        LOG.fail_if(cret != 0, f'cifre return code is {cret}')

        # Dataset
        dataset = CifreDataset(LOG, train_dir)

        train_args = {
            'num_epochs' : FLAGS.epoch_count,
            'batch_size' : train_batch_size,
            'lr' : FLAGS.train_lr }
        manager.train(train_args, dataset)
        manager.save_checkpoint(train_dir, 'cpt.pth.tar', create_folder=False, overwrite=True)

        # Compare 'model_name' against the best model.
        if best_model_name is None:
            best_model_name = model_name
            best_model_cpt_dir = train_dir
        else:
            LOG.log(f'==== Compare {model_name} to {best_model_name} ====')
            game_count = model_winner_count = best_model_winner_count = draw_count = 0

            # Give each player a chance to go first...
            for starting_idx in [0, 1]:
                report_file = train_dir / f'cmp{starting_idx}.json'
                player1_name = model_name if starting_idx == 0 else best_model_name
                player2_name = best_model_name if starting_idx == 0 else model_name

                cmp_cmd = [
                    f'../build/install/bin/{FLAGS.game}',
                    '--verbose' if FLAGS.verbose else '',
                    '--report-file', report_file,
                    '--parallel', str(game_parallel),
                    '--game-shape', argify_shape(game_shape, ','),
                    f'--game-count={FLAGS.cmp_game_count}',
                    filter_specified('--max-turns-per-game', FLAGS.max_turns_per_game),
                    '--player1=alphazero', '--name', player1_name, '--alphazero-model', player1_name,
                    '--player2=alphazero', '--name', player2_name, '--alphazero-model', player2_name ]

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
                    LOG.log(f"{player1_name} starting {j['game-results']['count']} games: "
                            f"{player1_name} {j['game-results']['player1']}, "
                            f"{player2_name} {j['game-results']['player2']}, "
                            f"draw {j['game-results']['draw']}")
                    game_count += j['game-results']['count']
                    model_winner_count += j['game-results']['player1' if starting_idx == 0 else 'player2']
                    best_model_winner_count += j['game-results']['player2' if starting_idx == 0 else 'player1']
                    draw_count += j['game-results']['draw']

            LOG.log(f'total {game_count} games: {model_name} {model_winner_count}, '
                    f'{best_model_name} {best_model_winner_count}, draw {draw_count}')
            if model_winner_count >= best_model_winner_count:
                LOG.log(f'best model now {model_name}', flush=True)
                best_model_name = model_name
                best_model_cpt_dir = train_dir
            else:
                LOG.log(f'keeping best model {best_model_name}', flush=True)
                manager.load_checkpoint(best_model_cpt_dir, 'cpt.pth.tar')

    LOG.log(f'==== Training complete, best model {best_model_name} ====')

if __name__ == '__main__':
    main()
