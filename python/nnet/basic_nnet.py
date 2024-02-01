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
import torch.optim as optim

#
# Basic NNet. 'args' is a dict that must include 'num_rows',
# 'num_cols', 'num_actions', 'num_channels', 'dropout'.
#
class CifreBasicNNet(nn.Module):
    def __init__(self, args):
        self.args = args

        super(CifreBasicNNet, self).__init__()
        self.conv1 = nn.Conv2d(1, self.args['num_channels'], 3, stride=1, padding=1)
        self.conv2 = nn.Conv2d(self.args['num_channels'], self.args['num_channels'], 3, stride=1, padding=1)
        self.conv3 = nn.Conv2d(self.args['num_channels'], self.args['num_channels'], 3, stride=1)
        self.conv4 = nn.Conv2d(self.args['num_channels'], self.args['num_channels'], 3, stride=1)

        self.bn1 = nn.BatchNorm2d(self.args['num_channels'])
        self.bn2 = nn.BatchNorm2d(self.args['num_channels'])
        self.bn3 = nn.BatchNorm2d(self.args['num_channels'])
        self.bn4 = nn.BatchNorm2d(self.args['num_channels'])

        self.fc1 = nn.Linear(self.args['num_channels']*(self.args['num_cols']-4)*(self.args['num_rows']-4), 1024)
        self.fc_bn1 = nn.BatchNorm1d(1024)

        self.fc2 = nn.Linear(1024, 512)
        self.fc_bn2 = nn.BatchNorm1d(512)

        self.fc3 = nn.Linear(512, self.args['num_actions'])

        self.fc4 = nn.Linear(512, 1)

    def forward(self, s):
        #                                             s: batch_size x args.num_cols x args.num_rows
        s = s.view(-1, 1, self.args['num_cols'], self.args['num_rows'])  # batch_size x 1 x args.num_cols x args.num_rows
        s = F.relu(self.bn1(self.conv1(s)))            # batch_size x num_channels x args.num_cols x args.num_rows
        s = F.relu(self.bn2(self.conv2(s)))            # batch_size x num_channels x args.num_cols x args.num_rows
        s = F.relu(self.bn3(self.conv3(s)))            # batch_size x num_channels x (.args.num_cols-2) x (args.num_rows-2)
        s = F.relu(self.bn4(self.conv4(s)))            # batch_size x num_channels x (.args.num_cols-4) x (args.num_rows-4)
        s = s.view(-1, self.args['num_channels']*(self.args['num_cols']-4)*(self.args['num_rows']-4))

        # batch_size x 1024
        s = F.dropout(F.relu(self.fc_bn1(self.fc1(s))), p=self.args['dropout'], training=self.training)
        # batch_size x 512
        s = F.dropout(F.relu(self.fc_bn2(self.fc2(s))), p=self.args['dropout'], training=self.training)

        # batch_size x args.num_actions
        pi = self.fc3(s)
        # batch_size x 1
        v = self.fc4(s)

        return F.log_softmax(pi, dim=1), torch.tanh(v)
