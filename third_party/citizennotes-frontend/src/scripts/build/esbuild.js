// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check

const path = require('path');

const citizennotes_paths = require('../citizennotes_paths.js');
const citizennotes_plugin = require('./citizennotes_plugin.js');

// esbuild module uses binary in this path.
process.env.ESBUILD_BINARY_PATH = path.join(citizennotes_paths.citizennotesRootPath(), 'third_party', 'esbuild', 'esbuild');

const entryPoints = [process.argv[2]];
const outfile = process.argv[3];
const useSourceMaps = process.argv.slice(4).includes('--configSourcemaps');

const outdir = path.dirname(outfile);

const plugin = {
  name: 'citizennotes-plugin',
  setup(build) {
    // https://esbuild.github.io/plugins/#on-resolve
    build.onResolve({filter: /.*/}, citizennotes_plugin.esbuildPlugin(outdir));
  },
};

require('esbuild')
    .build({
      entryPoints,
      outfile,
      bundle: true,
      format: 'esm',
      platform: 'browser',
      plugins: [plugin],
      sourcemap: useSourceMaps,
    })
    .catch(err => {
      console.error('failed to run esbuild:', err);
      process.exit(1);
    });
