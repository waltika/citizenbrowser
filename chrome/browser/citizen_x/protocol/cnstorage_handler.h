// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_PROTOCOL_STORAGE_HANDLER_H_
#define CHROME_BROWSER_CITIZENNOTES_PROTOCOL_STORAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/citizen_x/protocol/storage.h"

namespace content {
class WebContents;
}  // namespace content

class CNStorageHandler : public protocol::Storage::Backend {
 public:
  CNStorageHandler(content::WebContents* web_contents,
                 protocol::UberDispatcher* dispatcher);

  CNStorageHandler(const CNStorageHandler&) = delete;
  CNStorageHandler& operator=(const CNStorageHandler&) = delete;

  ~CNStorageHandler() override;

 private:
  void RunBounceTrackingMitigations(
      std::unique_ptr<RunBounceTrackingMitigationsCallback> callback) override;

  static void GotDeletedSites(
      std::unique_ptr<RunBounceTrackingMitigationsCallback> callback,
      const std::vector<std::string>& sites);

  base::WeakPtr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_PROTOCOL_STORAGE_HANDLER_H_
