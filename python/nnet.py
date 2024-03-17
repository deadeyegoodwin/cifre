# Copyright 2024, David Goodwin. All rights reserved.

import os
import random
import sys
import time

import numpy as np
import torch
import torch.onnx
from torch.utils.data import DataLoader
from tqdm import tqdm

#
# Manage a neural network.
#
class CifreNNetManager(object):
    def __init__(self, nnet, nnet_input_shape, repodir):
        """
        Initialize for 'nnet'.
        """
        self._nnet = nnet
        self._nnet_input_shape = nnet_input_shape
        self._repodir = repodir
        self._optimizer = torch.optim.Adam(self._nnet.parameters())
        self._epoch = 0
        self._use_cuda = torch.cuda.is_available()
        if self._use_cuda:
            self._nnet.cuda()

    def train(self, train_args, dataset):
        """
        Train network using CifreDataset 'dataset'.  'train_args' should
        be a dict that includes entries for 'num_epochs' and
        'batch_size'.
        """
        self._nnet.train()
        dataloader = DataLoader(
            dataset, batch_size=train_args['batch_size'], drop_last=True, shuffle=True)

        for _ in range(train_args['num_epochs']):
            print('Epoch ' + str(self._epoch))
            self._epoch += 1

            progress = tqdm(total=len(dataloader), desc='Train')
            for d_in, d_pi, d_v in dataloader:
                tensor_in = torch.FloatTensor(d_in)
                tensor_pi = torch.FloatTensor(d_pi)
                tensor_v = torch.FloatTensor(d_v)
                if self._use_cuda:
                    tensor_in = tensor_in.contiguous().cuda()
                    tensor_pi = tensor_pi.contiguous().cuda()
                    tensor_v = tensor_v.contiguous().cuda()

                # Run batch through network
                pi_result, v_result = self._nnet(tensor_in)

                # Compute loss...
                pi_loss = -torch.sum(tensor_pi * pi_result) / (tensor_pi.size(0))
                v_loss = torch.sum((tensor_v - v_result.view(-1)) ** 2) / (tensor_v.size(0))
                total_loss = pi_loss + v_loss

                # Backprop...
                self._optimizer.zero_grad()
                total_loss.backward()
                self._optimizer.step()

                progress.set_postfix(Loss_pi=pi_loss.item(), Loss_v=v_loss.item())
                progress.update(1)
            progress.close()

    def predict(self, board):
        """
        Run prediction for 'board', a 2d nparray representation of board.
        Return a tuple (pi, v).
        """
        board_tensor = torch.FloatTensor(board.astype(np.float32))
        if self._use_cuda:
            board_tensor = board_tensor.contiguous().cuda()

        self._nnet.eval()
        with torch.no_grad():
            pi, v = self._nnet(board_tensor.unsqueeze(0))
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
            'epoch': self._epoch,
            'model_state_dict': self._nnet.state_dict(),
            'optimizer_state_dict': self._optimizer.state_dict()
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

        checkpoint = torch.load(fullpath, map_location=None if self._use_cuda else 'cpu')
        self._nnet.load_state_dict(checkpoint['model_state_dict'])
        self._optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self._epoch = checkpoint['epoch']

    def save_triton_repository(self, model_name, model_version=1):
        """
        Save nnet as a onnxruntime model called 'model_name'.
        """
        if not os.path.isdir(self._repodir):
            raise NotADirectoryError(f"model directory {self._repodir} does not exist")
        model_dir = os.path.join(self._repodir, model_name)
        if os.path.exists(model_dir):
            raise FileExistsError(f"directory {model_dir} already exists")
        os.mkdir(model_dir)
        model_dir = os.path.join(model_dir, str(model_version))
        os.mkdir(model_dir)

        # Appropriately sized input.
        input_shape_with_batch = (1,) + self._nnet_input_shape
        board_tensor = torch.randn(input_shape_with_batch, requires_grad=True)
        if self._use_cuda:
            board_tensor = board_tensor.contiguous().cuda()

        # Export the model
        torch.onnx.export(
            self._nnet,
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
