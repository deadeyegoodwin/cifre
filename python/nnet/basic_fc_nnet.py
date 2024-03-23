# Copyright 2024, David Goodwin. All rights reserved.

import sys

import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F

#
# Basic Fully-Connected NNet. 'args' is a dict that must include
# 'num_rows', 'num_cols', 'num_actions', 'dropout'.
#
class CifreBasicFcNNet(nn.Module):
    def __init__(self, args):
        self.args = args

        super(CifreBasicFcNNet, self).__init__()
        self.fc1 = nn.Linear(self.args['num_cols'] * self.args['num_rows'], 1024)
        self.fc_bn1 = nn.BatchNorm1d(1024)

        self.fc2 = nn.Linear(1024, 512)
        self.fc_bn2 = nn.BatchNorm1d(512)

        self.fc3 = nn.Linear(512, self.args['num_actions'])
        self.fc4 = nn.Linear(512, 1)

    def forward(self, s):
        # s: batch_size x args.num_cols x args.num_rows
        s = s.view(-1, self.args['num_cols'] * self.args['num_rows'])

        # batch_size x 1024
        s = F.dropout(F.relu(self.fc_bn1(self.fc1(s))), p=self.args['dropout'], training=self.training)
        # batch_size x 512
        s = F.dropout(F.relu(self.fc_bn2(self.fc2(s))), p=self.args['dropout'], training=self.training)

        # batch_size x args.num_actions
        pi = self.fc3(s)
        # batch_size x 1
        v = self.fc4(s)

        return F.log_softmax(pi, dim=1), torch.tanh(v)
