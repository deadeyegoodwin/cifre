# Copyright 2024, David Goodwin. All rights reserved.

import os
import random
import sys
import time

import numpy as np
import torch
import torch.onnx
from tqdm import tqdm

#
# Manage a neural network.
#
class CifreNNetManager(object):
    def __init__(self, nnet, nnet_input_shape, repodir):
        """
        Initialize for 'nnet'.
        """
        self.nnet = nnet
        self.nnet_input_shape = nnet_input_shape
        self.repodir = repodir
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
            if os.path.exists(folder):
                raise FileExistsError(f"{folder} exists and is not a directory")
            os.mkdir(folder)
        if not os.path.isdir(folder):
            raise NotADirectoryError(f"directory {folder} does not exist")

        fullpath = os.path.join(folder, filename)
        if (not overwrite) and os.path.exists(fullpath):
            raise FileExistsError(f"{fullpath} exists, cannot overwrite")

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
        if not os.path.isdir(folder):
            raise NotADirectoryError(f"directory {folder} does not exist")
        fullpath = os.path.join(folder, filename)
        if not os.path.exists(fullpath):
            raise FileNotFoundError(f"{fullpath} does not exist")

        checkpoint = torch.load(fullpath, map_location=None if self.use_cuda else 'cpu')
        self.nnet.load_state_dict(checkpoint['model_state_dict'])
        self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self.epoch = checkpoint['epoch']

    def save_triton_repository(self, model_name, model_version=1):
        """
        Save nnet as a onnxruntime model called 'model_name'.
        """
        if not os.path.isdir(self.repodir):
            raise NotADirectoryError(f"model directory {self.repodir} does not exist")
        model_dir = os.path.join(self.repodir, model_name)
        if os.path.exists(model_dir):
            raise FileExistsError(f"directory {model_dir} already exists")
        os.mkdir(model_dir)
        model_dir = os.path.join(model_dir, str(model_version))
        os.mkdir(model_dir)

        # Appropriately sized input.
        input_shape_with_batch = (1,) + self.nnet_input_shape
        board_tensor = torch.randn(input_shape_with_batch, requires_grad=True)
        if self.use_cuda:
            board_tensor = board_tensor.contiguous().cuda()

        # Export the model
        torch.onnx.export(
            self.nnet,
            board_tensor,
            os.path.join(model_dir, "model.onnx"),
            export_params=True,        # store the trained parameter weights inside the model file
            opset_version=10,          # the ONNX version to export the model to
            do_constant_folding=True,  # whether to execute constant folding for optimization
            input_names = ['board'],   # the model's input names
            output_names = ['pi', 'v'], # the model's output names
            dynamic_axes={'board' : {0 : 'batch_size'},    # variable length axes
                          'pi' : {0 : 'batch_size'},
                          'v' : {0 : 'batch_size'}})
