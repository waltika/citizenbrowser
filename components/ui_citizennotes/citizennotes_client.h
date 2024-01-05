// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_CLIENT_H_
#define COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_CLIENT_H_

#include <string>

#include "components/ui_citizennotes/citizennotes_base_agent.h"
#include "components/ui_citizennotes/citizennotes_export.h"
#include "components/ui_citizennotes/dom.h"
#include "components/ui_citizennotes/forward.h"
#include "components/ui_citizennotes/protocol.h"

namespace ui_citizennotes {

class UiCitizenNotesServer;

// Every UI component that wants to be inspectable must instantiate
// this class and attach the corresponding backends/frontends (i.e: DOM, CSS,
// etc). This client is then attached to the UiCitizenNotesServer and all messages
// from this client are sent over the web socket owned by the server.
class UI_CITIZENNOTES_EXPORT UiCitizenNotesClient : public protocol::FrontendChannel {
 public:
  static const int kNotConnected = -1;

  UiCitizenNotesClient(const std::string& name, UiCitizenNotesServer* server);

  UiCitizenNotesClient(const UiCitizenNotesClient&) = delete;
  UiCitizenNotesClient& operator=(const UiCitizenNotesClient&) = delete;

  ~UiCitizenNotesClient() override;

  void AddAgent(std::unique_ptr<UiCitizenNotesAgent> agent);
  void Disconnect();
  void Dispatch(const std::string& json);

  bool connected() const;
  void set_connection_id(int connection_id);
  const std::string& name() const;

 private:
  void DisableAllAgents();
  void MaybeSendProtocolResponseOrNotification(
      std::unique_ptr<protocol::Serializable> message);

  // protocol::FrontendChannel
  void SendProtocolResponse(
      int callId,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  std::string name_;
  int connection_id_;

  std::vector<std::unique_ptr<UiCitizenNotesAgent>> agents_;
  protocol::UberDispatcher dispatcher_;
  RAW_PTR_EXCLUSION UiCitizenNotesServer* server_;
};

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_CITIZENNOTES_CLIENT_H_
