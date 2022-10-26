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

cxx = 'badcxx'
cxxflags = []
ldflags = []

if user_os == 'Darwin':
    cxx = 'clang++'
    opt = '-O2' if RELEASE else '-O0'
    cxxflags += ['-obuild/robosim', '-std=c++20', '-Wall', '-Wextra', '-pedantic', opt, '-Isrc', '-Iinclude']
    ldflags += [opt, '-lraylib', '-lglfw3', '-framework', 'Cocoa', '-framework', 'IOKit']
    if user_arch == 'arm64':
        ldflags += ['-Llib/mac-arm64']

for ext in ['*.o']:
    [os.remove(f) for f in glob.iglob('build/**/' + ext, recursive=True)]

os.makedirs('build', exist_ok=True)

print('Compiling...')

cfiles = glob.glob('src/**/*.cpp', recursive=True)
subprocess.run(
    [cxx]
    + cxxflags
    + cfiles
    + ldflags
)
