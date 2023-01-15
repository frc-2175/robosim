#!/usr/bin/env python3

import glob
import os
import platform
import subprocess
import sys

RELEASE = len(sys.argv) > 1 and sys.argv[1] == 'release'

user_os = platform.system().lower()
user_arch = platform.machine().lower()
print('Compiling for OS: {}, arch: {}'.format(user_os, user_arch))

cxx = 'badcxx'
cxxflags = []
ldflags = []

if user_os == 'windows':
    cxx = 'cl'
    opt = '/O2' if RELEASE else '/Od'
    cxxflags += [
        '/Fe:', 'robosim', '/Fd:', 'robosim',
        '/std:c++20', opt,
        '/I..\\src', '/I..\\include',
    ]

    def winlib(name):
        return '..\\lib\\windows-amd64\\' + name

    ldflags += [
        '/Zi',
        '/MT', # use static multithreaded runtime library
        winlib('raylib.lib'), winlib('box2d.lib'), winlib('glfw3_mt.lib'),
        'gdi32.lib', 'shell32.lib', 'winmm.lib',
    ]
elif user_os == 'darwin' or user_os == 'linux':
    cxx = 'clang++'
    opt = '-O2' if RELEASE else '-O0'
    cxxflags += [
        '-orobosim',
        '-std=c++20', opt, '-Wall', '-Wextra', '-pedantic',
        '-I../src', '-I../include',
    ]
    ldflags += [
        '-lraylib', '-lbox2d',
    ]
    if user_os == 'darwin':
        ldflags += ['-framework', 'Cocoa', '-framework', 'IOKit']

if user_os == 'windows' and user_arch == 'amd64':
    pass # todo: library search location? would be nice to remove the winlib nonsense from above
elif user_os == 'darwin' and user_arch == 'arm64':
    ldflags += ['-lglfw3']
    ldflags += ['-L../lib/mac-arm64']
elif user_os == 'linux' and user_arch == 'amd64':
    ldflags += ['-L../lib/linux-amd64']
else:
    print('Unknown OS/arch combo, libraries will probably be missing!')
    print('OS: {}, arch: {}'.format(user_os, user_arch))

os.makedirs('build', exist_ok=True)
os.chdir('build')
cfiles = glob.glob('../src/**/*.cpp', recursive=True)
subprocess.run(
    [cxx]
    + cxxflags
    + cfiles
    + ldflags
)
