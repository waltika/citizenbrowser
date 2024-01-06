// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_CITIZENNOTES_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

class CNRemoteDebuggingServer {
 public:
  static void EnableTetheringForDebug();

  CNRemoteDebuggingServer();

  CNRemoteDebuggingServer(const CNRemoteDebuggingServer&) = delete;
  CNRemoteDebuggingServer& operator=(const CNRemoteDebuggingServer&) = delete;

  virtual ~CNRemoteDebuggingServer();
};

#endif  // CHROME_BROWSER_CITIZENNOTES_REMOTE_DEBUGGING_SERVER_H_
