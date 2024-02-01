# Copyright 2024, David Goodwin. All rights reserved.

import os
import random
import sys
import time

import numpy as np
import torch
from tqdm import tqdm

#
# Manage a neural network.
#
class CifreNNetManager(object):
    def __init__(self, nnet):
        """
        Initialize for 'nnet'.
        """
        self.nnet = nnet
        self.optimizer = torch.optim.Adam(self.nnet.parameters())
        self.epoch = 0
        self.use_cuda = torch.cuda.is_available()
        if self.use_cuda:
            self.nnet.cuda()

    def train(self, train_args, dataset):
        """
        Train network using 'dataset'. 'dataset' consists of list of
        tuples (2d nparray of board, nparray probabilities, v).
        'train_args' should be a dict that includes entries for
        'num_epochs' and 'batch_size'.
        """
        num_batches = int(len(dataset) / train_args['batch_size'])

        for _ in range(train_args['num_epochs']):
            print('Epoch ' + str(self.epoch))
            self.epoch += 1

            self.nnet.train()
            dataset_idx = random.sample(range(0, len(dataset)), len(dataset))

            progress = tqdm(range(num_batches), desc='Train')
            for bidx in progress:
                dataset_idx_start = bidx * train_args['batch_size']
                dataset_idx_end = dataset_idx_start + train_args['batch_size']
                board_list, pi_list, v_list = list(zip(
                    *[dataset[i] for i in dataset_idx[dataset_idx_start:dataset_idx_end]]))

                board_batch = torch.FloatTensor(np.array(board_list).astype(np.float32))
                pi_batch = torch.FloatTensor(np.array(pi_list))
                v_batch = torch.FloatTensor(np.array(v_list).astype(np.float32))
                if self.use_cuda:
                    board_batch = board_batch.contiguous().cuda()
                    pi_batch = pi_batch.contiguous().cuda()
                    v_batch = v_batch.contiguous().cuda()

                # Run batch through network
                pi_result, v_result = self.nnet(board_batch)

                # Compute loss...
                pi_loss = -torch.sum(pi_batch * pi_result) / (pi_batch.size(0))
                v_loss = torch.sum((v_batch - v_result.view(-1)) ** 2) / (v_batch.size(0))
                total_loss = pi_loss + v_loss

                progress.set_postfix(Loss_pi=pi_loss.item(), Loss_v=v_loss.item())

                # Backprop...
                self.optimizer.zero_grad()
                total_loss.backward()
                self.optimizer.step()

    def predict(self, board):
        """
        Run prediction for 'board', a 2d nparray representation of board.
        Return a tuple (pi, v).
        """
        board_tensor = torch.FloatTensor(board.astype(np.float32))
        if self.use_cuda:
            board_tensor = board_tensor.contiguous().cuda()

        self.nnet.eval()
        with torch.no_grad():
            pi, v = self.nnet(board_tensor.unsqueeze(0))
        return torch.exp(pi).data.cpu().numpy()[0], v.data.cpu().numpy()[0]

    def save_checkpoint(self, folder, filename, create_folder=False, overwrite=False):
        """
        Save the nnet checkpoint to 'filename' in the specified
        os.path 'folder'.
        """
        if create_folder and (not os.path.isdir(folder)):
            assert not os.path.exists(folder), f"{folder} exists and is not a directory"
            os.mkdir(folder)
        assert os.path.isdir(folder), f"directory {folder} does not exist"

        fullpath = os.path.join(folder, filename)
        if not overwrite:
            assert not os.path.exists(fullpath), f"{fullpath} exists, cannot overwrite"

        torch.save({
            'epoch': self.epoch,
            'model_state_dict': self.nnet.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict()
        }, fullpath)

    def load_checkpoint(self, folder, filename):
        """
        Load the nnet checkpoint from 'filename' in the specified
        os.path 'folder'.
        """
        assert os.path.isdir(folder), f"directory {folder} does not exist"
        fullpath = os.path.join(folder, filename)
        assert os.path.exists(fullpath), f"{fullpath} does not exist"

        checkpoint = torch.load(fullpath, map_location=None if self.use_cuda else 'cpu')
        self.nnet.load_state_dict(checkpoint['model_state_dict'])
        self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self.epoch = checkpoint['epoch']
