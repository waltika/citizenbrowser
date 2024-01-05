// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_AUTO_ATTACHER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_AUTO_ATTACHER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"

namespace content {

class CitizenNotesAgentHost;
class CitizenNotesAgentHostImpl;
class CitizenNotesRendererChannel;
class NavigationHandle;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameCitizenNotesAgentHost;

namespace protocol {

class CNTargetAutoAttacher {
 public:
  class Client : public base::CheckedObserver {
   public:
    virtual bool AutoAttach(CNTargetAutoAttacher* source,
                            CitizenNotesAgentHost* host,
                            bool waiting_for_debugger) = 0;
    virtual void AutoDetach(CNTargetAutoAttacher* source,
                            CitizenNotesAgentHost* host) = 0;
    virtual void SetAttachedTargetsOfType(
        CNTargetAutoAttacher* source,
        const base::flat_set<scoped_refptr<CitizenNotesAgentHost>>& hosts,
        const std::string& type) = 0;
    virtual void AutoAttacherDestroyed(CNTargetAutoAttacher* auto_attacher) = 0;
    virtual std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
        CNTargetAutoAttacher* auto_attacher,
        NavigationHandle* navigation_handle) = 0;
    virtual void TargetInfoChanged(CitizenNotesAgentHost* host) = 0;

   protected:
    Client() = default;
    ~Client() override = default;
  };

  CNTargetAutoAttacher(const CNTargetAutoAttacher&) = delete;
  CNTargetAutoAttacher& operator=(const CNTargetAutoAttacher&) = delete;

  virtual ~CNTargetAutoAttacher();

  void AddClient(Client* client,
                 bool wait_for_debugger_on_start,
                 base::OnceClosure callback);
  void RemoveClient(Client* client);
  void UpdateWaitForDebuggerOnStart(Client* client,
                                    bool wait_for_debugger_on_start,
                                    base::OnceClosure callback);

  void AppendNavigationThrottles(
      NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<NavigationThrottle>>* throttles);

  scoped_refptr<RenderFrameCitizenNotesAgentHost> HandleNavigation(
      NavigationRequest* navigation_request,
      bool wait_for_debugger_on_start);

 protected:
  using Hosts = base::flat_set<scoped_refptr<CitizenNotesAgentHost>>;

  CNTargetAutoAttacher();

  bool auto_attach() const;
  bool wait_for_debugger_on_start() const;

  virtual void UpdateAutoAttach(base::OnceClosure callback);

  void DispatchAutoAttach(CitizenNotesAgentHost* host, bool waiting_for_debugger);
  void DispatchAutoDetach(CitizenNotesAgentHost* host);
  void DispatchSetAttachedTargetsOfType(
      const base::flat_set<scoped_refptr<CitizenNotesAgentHost>>& hosts,
      const std::string& type);
  void DispatchTargetInfoChanged(CitizenNotesAgentHost* host);

 private:
  base::ObserverList<Client, false, true> clients_;
  base::flat_set<Client*> clients_requesting_wait_for_debugger_;
};

class CNRendererAutoAttacherBase : public CNTargetAutoAttacher {
 public:
  explicit CNRendererAutoAttacherBase(CitizenNotesRendererChannel* renderer_channel);
  ~CNRendererAutoAttacherBase() override;

 protected:
  void UpdateAutoAttach(base::OnceClosure callback) override;
  void ChildWorkerCreated(CitizenNotesAgentHostImpl* agent_host,
                          bool waiting_for_debugger);

 private:
   RAW_PTR_EXCLUSION CitizenNotesRendererChannel* const renderer_channel_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_TARGET_AUTO_ATTACHER_H_
