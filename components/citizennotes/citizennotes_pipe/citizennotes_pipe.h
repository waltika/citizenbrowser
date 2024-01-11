// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_CITIZENNOTES_PIPE_CITIZENNOTES_PIPE_H_
#define COMPONENTS_DEVTOOLS_CITIZENNOTES_PIPE_CITIZENNOTES_PIPE_H_

namespace citizennotes_pipe {

// Checks if DevTools remote debugging pipe file descriptros are open. Embedders
// are supposed to call this before opening any files if --remote-debugging-pipe
// switch is present, see: https://crbug.com/1423048
bool AreFileDescriptorsOpen();

}  // namespace citizennotes_pipe

#endif  // COMPONENTS_DEVTOOLS_CITIZENNOTES_PIPE_CITIZENNOTES_PIPE_H_
