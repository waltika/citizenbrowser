// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DEVICE_ACCESS_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DEVICE_ACCESS_HANDLER_H_

#include "content/browser/citizen_x/citizennotes_device_request_prompt_info.h"
#include "content/browser/citizen_x/protocol/device_access.h"
#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"

namespace content {

namespace protocol {

class CNDeviceAccessHandler : public CitizenNotesDomainHandler,
                            public DeviceAccess::Backend {
 public:
  CNDeviceAccessHandler();
  ~CNDeviceAccessHandler() override;

  static std::vector<CNDeviceAccessHandler*> ForAgentHost(
      CitizenNotesAgentHostImpl* host);
  void Wire(UberDispatcher* dispatcher) override;

  CNDeviceAccessHandler(const CNDeviceAccessHandler&) = delete;
  CNDeviceAccessHandler& operator=(const CNDeviceAccessHandler&) = delete;

  void UpdateDeviceRequestPrompt(CitizennotesDeviceRequestPromptInfo* prompt_info);
  void CleanUpDeviceRequestPrompt(CitizennotesDeviceRequestPromptInfo* prompt_info);

 private:
  DispatchResponse Enable() override;
  DispatchResponse Disable() override;
  DispatchResponse SelectPrompt(const String& in_id,
                                const String& in_deviceId) override;
  DispatchResponse CancelPrompt(const String& in_id) override;

  const std::string& FindOrAddRequestId(
      CitizennotesDeviceRequestPromptInfo* prompt_info);
  CitizennotesDeviceRequestPromptInfo* FindRequest(const String& requestId);

  absl::optional<DeviceAccess::Frontend> frontend_;

  bool enabled_ = false;

  // Map to PromptInfo instances until CleanUp is called, removing them
  // from the map.
  base::flat_map<std::string /*id*/, raw_ptr<CitizennotesDeviceRequestPromptInfo>>
      request_prompt_infos_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_DEVICE_ACCESS_HANDLER_H_
