// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_IO_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_IO_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/io.h"

namespace content {
class BrowserContext;
class CitizenNotesIOContext;
class RenderFrameHostImpl;
class StoragePartition;

namespace protocol {

class CNIOHandler : public CitizenNotesDomainHandler,
                  public IO::Backend {
 public:
  explicit CNIOHandler(CitizenNotesIOContext* io_context);

  CNIOHandler(const CNIOHandler&) = delete;
  CNIOHandler& operator=(const CNIOHandler&) = delete;

  ~CNIOHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  // Protocol methods.
  void Read(
      const std::string& handle,
      Maybe<int> offset,
      Maybe<int> max_size,
      std::unique_ptr<ReadCallback> callback) override;
  Response Close(const std::string& handle) override;

 private:
  void ReadComplete(std::unique_ptr<ReadCallback> callback,
                    std::unique_ptr<std::string> data,
                    bool base64_encoded,
                    int status);

  std::unique_ptr<IO::Frontend> frontend_;
  RAW_PTR_EXCLUSION CitizenNotesIOContext* io_context_;
  raw_ptr<BrowserContext> browser_context_;
  RAW_PTR_EXCLUSION StoragePartition* storage_partition_;
  base::WeakPtrFactory<CNIOHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_IO_HANDLER_H_
