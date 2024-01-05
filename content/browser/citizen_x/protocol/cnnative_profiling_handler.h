// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_NATIVE_PROFILING_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_NATIVE_PROFILING_HANDLER_H_

#include "content/browser/citizen_x/protocol/devtools_domain_handler.h"
#include "content/browser/citizen_x/protocol/native_profiling.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
namespace protocol {

// Handle the DevTool native profiling commands. This is currently used only
// for the PGO instrumented build.
class CNNativeProfilingHandler final : public CitizenNotesDomainHandler,
                                     public NativeProfiling::Backend {
 public:
  CNNativeProfilingHandler();
  CNNativeProfilingHandler(const CNNativeProfilingHandler& other) = delete;
  CNNativeProfilingHandler& operator=(const CNNativeProfilingHandler& other) =
      delete;
  ~CNNativeProfilingHandler() override;

  // CitizenNotesDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // NativeProfiling::Backend.
  void DumpProfilingDataOfAllProcesses(
      std::unique_ptr<DumpProfilingDataOfAllProcessesCallback> callback)
      override;

 private:
  std::unique_ptr<NativeProfiling::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_NATIVE_PROFILING_HANDLER_H_
