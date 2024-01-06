// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_FILE_WATCHER_H_
#define CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_FILE_WATCHER_H_

#include <vector>

#include "base/functional/callback.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

class CitizenNotesFileWatcher {
 public:
  struct Deleter {
    void operator()(const CitizenNotesFileWatcher* ptr);
  };

  using WatchCallback =
      base::RepeatingCallback<void(const std::vector<std::string>&,
                                   const std::vector<std::string>&,
                                   const std::vector<std::string>&)>;
  CitizenNotesFileWatcher(
      WatchCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  CitizenNotesFileWatcher(const CitizenNotesFileWatcher&) = delete;
  CitizenNotesFileWatcher& operator=(const CitizenNotesFileWatcher&) = delete;

  void AddWatch(base::FilePath path);
  void RemoveWatch(base::FilePath path);

 private:
  ~CitizenNotesFileWatcher();  // Use Deleter to destroy objects of this type.
  class SharedFileWatcher;
  static SharedFileWatcher* s_shared_watcher_;

  void Destroy() const { delete this; }
  void InitSharedWatcher();
  void AddWatchOnImpl(base::FilePath path);
  void RemoveWatchOnImpl(base::FilePath path);

  scoped_refptr<SharedFileWatcher> shared_watcher_;
  WatchCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_FILE_WATCHER_H_
