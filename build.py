#!/usr/bin/env python3

import glob
import os
import platform
import random
import re
import shutil
import string
import subprocess
import sys

RELEASE = len(sys.argv) > 1 and sys.argv[1] == 'release'

user_os = platform.system()
user_arch = platform.machine()
print('Compiling for OS: {}, arch: {}'.format(user_os, user_arch))

cxx = 'badcxx'
cxxflags = []
ldflags = []

if user_os == 'Darwin' or user_os == 'Linux':
    cxx = 'clang++'
    opt = '-O2' if RELEASE else '-O0'
    cxxflags += [
        '-obuild/robosim',
        '-std=c++20', opt, '-Wall', '-Wextra', '-pedantic',
        '-Isrc', '-Iinclude',
    ]
    ldflags += [
        '-lraylib',
    ]
    if user_os == 'Darwin':
        ldflags += ['-framework', 'Cocoa', '-framework', 'IOKit']

if user_os == 'Darwin' and user_arch == 'arm64':
    ldflags += ['-lglfw3']
    ldflags += ['-Llib/mac-arm64']
elif user_os == 'Linux' and user_arch == 'amd64':
    ldflags += ['-Llib/linux-amd64']
else:
    print('Unknown OS/arch combo, libraries will probably be missing!')
    print('OS: {}, arch: {}'.format(user_os, user_arch))

os.makedirs('build', exist_ok=True)
cfiles = glob.glob('src/**/*.cpp', recursive=True)
subprocess.run(
    [cxx]
    + cxxflags
    + cfiles
    + ldflags
)
