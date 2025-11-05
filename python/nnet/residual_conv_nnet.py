# MIT License
#
# Copyright (c) 2018 Surag Nair
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys

import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F

#
# Residual Convolution NNet, conceptually similar to that used by
# original AlphaZero.  'args' is a dict that must include 'num_rows',
# 'num_cols', 'num_planes', 'num_residual_blocks', 'num_actions',
# 'num_channels'.
#
class CifreResidualConvNNet(nn.Module):
    def __init__(self, args):
        self.args = args
        self.input_size = self.args['num_rows'] * self.args['num_cols'] * self.args['num_planes']

        super(CifreResidualConvNNet, self).__init__()

        # Initial convolution
        self.conv1 = nn.Conv3d(1, self.args['num_channels'], kernel_size=3, stride=1, padding=1)
        self.bn1 = nn.BatchNorm3d(self.args['num_channels'])

        # Residual blocks
        self.res_blocks = nn.ModuleList(
            [self._create_res_block() for _ in range(self.args['num_residual_blocks'])])

        # Policy head
        self.policy_conv = nn.Conv3d(self.args['num_channels'], 2, kernel_size=1, stride=1)
        self.policy_bn = nn.BatchNorm3d(2)
        self.policy_fc = nn.Linear(2 * self.input_size, self.args['num_actions'])

        # Value head
        self.value_conv = nn.Conv3d(self.args['num_channels'], 1, kernel_size=1, stride=1)
        self.value_bn = nn.BatchNorm3d(1)
        self.value_fc1 = nn.Linear(self.input_size, 20)
        self.value_fc2 = nn.Linear(20, 1)

    def _create_res_block(self):
        return nn.Sequential(
            nn.Conv3d(self.args['num_channels'], self.args['num_channels'], kernel_size=3, stride=1, padding=1),
            nn.BatchNorm3d(self.args['num_channels']),
            nn.ReLU(),
            nn.Conv3d(self.args['num_channels'], self.args['num_channels'], kernel_size=3, stride=1, padding=1),
            nn.BatchNorm3d(self.args['num_channels']))

    def forward(self, s):
        # s: batch_size x args.num_rows x args.num_cols x args.num_planes

        # s: batch_size x 1 x args.num_rows x args.num_cols x args.num_planes
        s = s.view(-1, 1, self.args['num_rows'], self.args['num_cols'], self.args['num_planes'])

        # Initial block
        s = F.relu(self.bn1(self.conv1(s)))

        # Residual blocks
        for block in self.res_blocks:
            s = F.relu(s + block(s)) # Add skip connection before ReLU

        # Policy head
        pi = F.relu(self.policy_bn(self.policy_conv(s)))
        pi = pi.view(-1, 2 * self.input_size)
        pi = F.log_softmax(self.policy_fc(pi), dim=1)

        # Value head
        v = F.relu(self.value_bn(self.value_conv(s)))
        v = v.view(-1, self.input_size)
        v = F.relu(self.value_fc1(v))
        v = torch.tanh(self.value_fc2(v)) # v set to [-1,  1]

        return pi, v
