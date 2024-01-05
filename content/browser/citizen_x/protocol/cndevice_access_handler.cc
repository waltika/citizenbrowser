// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cndevice_access_handler.h"

namespace content {
namespace protocol {

// static
std::vector<CNDeviceAccessHandler*> CNDeviceAccessHandler::ForAgentHost(
    CitizenNotesAgentHostImpl* host) {
  return host->HandlersByName<CNDeviceAccessHandler>(
      DeviceAccess::Metainfo::domainName);
}

CNDeviceAccessHandler::CNDeviceAccessHandler()
    : CitizenNotesDomainHandler(DeviceAccess::Metainfo::domainName) {}
CNDeviceAccessHandler::~CNDeviceAccessHandler() = default;

void CNDeviceAccessHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.emplace(dispatcher->channel());
  DeviceAccess::Dispatcher::wire(dispatcher, this);
}

DispatchResponse CNDeviceAccessHandler::Enable() {
  enabled_ = true;
  return Response::Success();
}

DispatchResponse CNDeviceAccessHandler::Disable() {
  enabled_ = false;
  request_prompt_infos_.clear();
  return Response::Success();
}

DispatchResponse CNDeviceAccessHandler::SelectPrompt(const String& in_id,
                                                   const String& in_deviceId) {
  if (!enabled_) {
    return Response::ServerError("DeviceAccess domain is not enabled");
  }

  auto* prompt_info = FindRequest(in_id);
  if (!prompt_info) {
    return Response::InvalidParams("Cannot find request with id");
  }

  if (!prompt_info->SelectDevice(in_deviceId)) {
    return Response::InvalidParams("Cannot find device with deviceId");
  }

  return Response::Success();
}

DispatchResponse CNDeviceAccessHandler::CancelPrompt(const String& in_id) {
  if (!enabled_) {
    return Response::ServerError("DeviceAccess domain is not enabled");
  }

  auto* prompt_info = FindRequest(in_id);
  if (!prompt_info) {
    return Response::InvalidParams("Cannot find request with id");
  }

  prompt_info->Cancel();
  return Response::Success();
}

const std::string& CNDeviceAccessHandler::FindOrAddRequestId(
    CitizennotesDeviceRequestPromptInfo* prompt_info) {
  auto it = base::ranges::find_if(
      request_prompt_infos_,
      [prompt_info](auto& pair) { return pair.second == prompt_info; });
  if (it != request_prompt_infos_.end()) {
    return it->first;
  }

  auto request_id = base::UnguessableToken::Create().ToString();
  auto did_insert =
      request_prompt_infos_.insert_or_assign(request_id, prompt_info);
  CHECK(did_insert.second);
  return did_insert.first->first;
}

CitizennotesDeviceRequestPromptInfo* CNDeviceAccessHandler::FindRequest(
    const String& requestId) {
  auto it = request_prompt_infos_.find(requestId);
  if (it != request_prompt_infos_.end()) {
    return it->second;
  }
  return nullptr;
}

void CNDeviceAccessHandler::UpdateDeviceRequestPrompt(
    CitizennotesDeviceRequestPromptInfo* prompt_info) {
  if (enabled_) {
    auto request_id = FindOrAddRequestId(prompt_info);

    auto devices = std::make_unique<
        std::vector<std::unique_ptr<DeviceAccess::PromptDevice>>>();
    for (auto& device : prompt_info->GetDevices()) {
      devices->push_back(DeviceAccess::PromptDevice::Create()
                             .SetId(device.id)
                             .SetName(device.name)
                             .Build());
    }

    frontend_->DeviceRequestPrompted(request_id, std::move(devices));
  }
}

void CNDeviceAccessHandler::CleanUpDeviceRequestPrompt(
    CitizennotesDeviceRequestPromptInfo* prompt_info) {
  if (enabled_) {
    base::EraseIf(request_prompt_infos_,
                  [&](auto& pair) { return pair.second == prompt_info; });
  }
}

}  // namespace protocol
}  // namespace content
