#!/usr/bin/env python

#
# This file is part of RefFS.
# 
# Copyright (c) 2020-2024 Yifei Liu
# Copyright (c) 2020-2024 Wei Su
# Copyright (c) 2020-2024 Erez Zadok
# Copyright (c) 2020-2024 Stony Brook University
# Copyright (c) 2020-2024 The Research Foundation of SUNY
# Original Copyright (C) Peter Watkins
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# RefFS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

from pathlib import Path
from subprocess import Popen, PIPE
import os
import sys
import time
import errno


stderrExpected = 'USAGE: fuse-cpp-ramfs MOUNTPOINT'
p = Popen('src/fuse-cpp-ramfs', stdout=PIPE, stderr=PIPE)
stdout, stderr = p.communicate()

# Check for no STDOUT
if stdout:
  sys.stderr.write('STDOUT not empty\n')
  sys.exit(-1)

# Check for proper error message
stderrStr = stderr.decode().rstrip()
if stderrStr != stderrExpected:
  sys.stderr.write('Wrong stderr\n')
  sys.stderr.write('Expected {} Actual {}\n'.format(stderrExpected, stderrStr))
  sys.exit(-1)

# The exit code should've indicated a failure
if p.returncode == 0:
  sys.stderr.write('Wrong exit code\n')
  sys.stderr.write('Expected != 0 Actual {}\n'.format(p.returncode))
  sys.exit(-1)

sys.exit(0)