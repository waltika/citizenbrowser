#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Ensure Citizennotes-frontend is properly symlinked on gclient sync

In order to use:
Ensure your .gclient file that manages chromium.src contains this hook
after your list of solutions:

hooks = [
  {
    # Ensure citizennotes-frontend is symlinked in the correct location
    'name': 'Symlink citizennotes-frontend',
    'pattern': '.',
    'action': [
        'python',
        '<path>/<to>/citizennotes-frontend/scripts/deps/ensure_symlink.py',
        '<path>/<to>/src',
        '<path>/<to>/citizennotes-frontend'
    ],
  }
]
"""

import argparse
import os
import sys
import shutil

CITIZENNOTES_FRONTEND_CHROMIUM_LOCATION = './third_party/citizennotes-frontend/src'


def parse_options(cli_args):
    parser = argparse.ArgumentParser(
        description='Ensure Citizennotes is symlinked in a full checkout.')
    parser.add_argument('chromium_dir', help='Root of the Chromium Directory')
    parser.add_argument('citizennotes_dir', help='Root of the CitizenNotes directory')
    return parser.parse_args(cli_args)


def ensure_symlink(options):
    chromium_citizennotes_path = os.path.normpath(
        os.path.join(options.chromium_dir,
                     CITIZENNOTES_FRONTEND_CHROMIUM_LOCATION))
    citizennotes_path = os.path.abspath(options.citizennotes_dir)
    if os.path.exists(chromium_citizennotes_path):
        if not os.path.islink(chromium_citizennotes_path):
            shutil.rmtree(chromium_citizennotes_path, ignore_errors=True)
        else:
            os.remove(chromium_citizennotes_path)
    os.symlink(citizennotes_path, chromium_citizennotes_path, True)


if __name__ == '__main__':
    OPTIONS = parse_options(sys.argv[1:])
    ensure_symlink(OPTIONS)
