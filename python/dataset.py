#!/usr/bin/env python3

# Copyright 2024, David Goodwin. All rights reserved.

import json
import os
import sys
import numpy as np
import torch
from torch.utils.data import Dataset

from log import CifreLogger

class CifreDataset(Dataset):
    def __init__(self, log, train_dir):
        self._log = log

        # Read training configuration json
        config_path = train_dir / 'train.json'
        with config_path.open('r', encoding ='utf-8') as f:
            j = json.load(f)

        # Get the shape of input and output tensors and confirm that
        # they all have same number of batches.
        full_input_shape = j['in_shape']
        self._log.fail_if(
            len(full_input_shape) < 2,
            'expecting input shape to have at least one dimension in addition to batch dimension')
        self._batch_size = full_input_shape[0]
        self._input_shape = full_input_shape[1:]

        full_pi_shape = j['pi_shape']
        self._log.fail_if(
            len(full_pi_shape) < 2,
            'expecting pi shape to have at least one dimension in addition to batch dimension')
        self._log.fail_if(
            full_pi_shape[0] != self._batch_size,
            f'pi batch size {full_pi_shape[0]} != input batch size {self._batch_size}')
        self._pi_shape = full_pi_shape[1:]

        full_v_shape = j['v_shape']
        self._log.fail_if(
            len(full_v_shape) < 2,
            'expecting v shape to have at least one dimension in addition to batch dimension')
        self._log.fail_if(
            full_v_shape[0] != self._batch_size,
            f'v batch size {full_v_shape[0]} != input batch size {self._batch_size}')
        self._v_shape = full_v_shape[1:]

        # Read input, pi and v data into numpy.
        self._input_tensor = np.fromfile(train_dir / 'in.bin', dtype=np.float32)
        self._input_tensor = np.reshape(self._input_tensor, full_input_shape)
        self._pi_tensor = np.fromfile(train_dir / 'pi.bin', dtype=np.float32)
        self._pi_tensor = np.reshape(self._pi_tensor, full_pi_shape)
        self._v_tensor = np.fromfile(train_dir / 'v.bin', dtype=np.float32)
        self._v_tensor = np.reshape(self._v_tensor, full_v_shape)

    def __len__(self):
        return self._batch_size

    def __getitem__(self, idx):
        return self._input_tensor[idx], self._pi_tensor[idx], self._v_tensor[idx]
