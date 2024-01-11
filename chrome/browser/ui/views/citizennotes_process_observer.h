// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CITIZENNOTES_PROCESS_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_CITIZENNOTES_PROCESS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace ui_citizennotes {
class TracingAgent;
}

// Observer that is notified of browser and gpu processes when they are launched
// and destroyed. This is used to update the process id for event tracing.
class CitizennotesProcessObserver : public content::BrowserChildProcessObserver {
 public:
  explicit CitizennotesProcessObserver(ui_citizennotes::TracingAgent* agent);

  CitizennotesProcessObserver(const CitizennotesProcessObserver&) = delete;
  CitizennotesProcessObserver& operator=(const CitizennotesProcessObserver&) = delete;

  ~CitizennotesProcessObserver() override;

 private:
  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  raw_ptr<ui_citizennotes::TracingAgent> tracing_agent_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CITIZENNOTES_PROCESS_OBSERVER_H_
