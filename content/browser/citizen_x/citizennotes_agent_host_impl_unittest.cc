// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/citizennotes/citizennotes_manager.h"
#include "content/browser/citizennotes/shared_worker_citizennotes_manager.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/citizennotes_agent_host.h"
#include "content/public/browser/citizennotes_external_agent_proxy.h"
#include "content/public/browser/citizennotes_external_agent_proxy_delegate.h"
#include "content/public/browser/citizennotes_manager_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestCitizenNotesClientHost : public CitizenNotesAgentHostClient {
 public:
  TestCitizenNotesClientHost() : closed_(false) {}

  TestCitizenNotesClientHost(const TestCitizenNotesClientHost&) = delete;
  TestCitizenNotesClientHost& operator=(const TestCitizenNotesClientHost&) = delete;

  ~TestCitizenNotesClientHost() override { EXPECT_TRUE(closed_); }

  void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    agent_host_->DetachClient(this);
    closed_ = true;
  }

  void AgentHostClosed(CitizenNotesAgentHost* agent_host) override { FAIL(); }

  void DispatchProtocolMessage(CitizenNotesAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}

  void InspectAgentHost(CitizenNotesAgentHost* agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
  }

  CitizenNotesAgentHost* agent_host() { return agent_host_.get(); }

  static void ResetCounters() { close_counter = 0; }

  static int close_counter;

 private:
  bool closed_;
  scoped_refptr<CitizenNotesAgentHost> agent_host_;
};

int TestCitizenNotesClientHost::close_counter = 0;

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the contents is hung.
  void RendererUnresponsive(
      WebContents* source,
      RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

class BrowserClient : public ContentBrowserClient {
 public:
  BrowserClient() = default;
  ~BrowserClient() override = default;
  std::unique_ptr<content::CitizenNotesManagerDelegate>
  CreateCitizenNotesManagerDelegate() override {
    return std::make_unique<CitizenNotesManagerDelegate>();
  }
};

}  // namespace

class CitizenNotesAgentHostImplTest : public RenderViewHostImplTestHarness {
 public:
  CitizenNotesAgentHostImplTest() {}

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    TestCitizenNotesClientHost::ResetCounters();
    browser_content_client_ = std::make_unique<BrowserClient>();
    original_client_ =
        SetBrowserClientForTesting(browser_content_client_.get());
    CitizenNotesManager::ShutdownForTests();
  }
  void TearDown() override {
    SetBrowserClientForTesting(original_client_);
    CitizenNotesManager::ShutdownForTests();

    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  std::unique_ptr<ContentBrowserClient> browser_content_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(CitizenNotesAgentHostImplTest, OpenAndManuallyCloseCitizenNotesClientHost) {
  scoped_refptr<CitizenNotesAgentHost> agent(
      CitizenNotesAgentHost::GetOrCreateFor(web_contents()));
  EXPECT_FALSE(agent->IsAttached());

  TestCitizenNotesClientHost client_host;
  client_host.InspectAgentHost(agent.get());
  // Test that the connection is established.
  EXPECT_TRUE(agent->IsAttached());
  EXPECT_EQ(0, TestCitizenNotesClientHost::close_counter);

  client_host.Close();
  EXPECT_EQ(1, TestCitizenNotesClientHost::close_counter);
  EXPECT_FALSE(agent->IsAttached());
}

TEST_F(CitizenNotesAgentHostImplTest, NoUnresponsiveDialogInInspectedContents) {
  const GURL url("http://www.google.com");
  contents()->NavigateAndCommit(url);
  TestRenderViewHost* inspected_rvh = test_rvh();
  EXPECT_TRUE(inspected_rvh->IsRenderViewLive());
  EXPECT_FALSE(contents()->GetDelegate());
  TestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestCitizenNotesClientHost client_host;
  scoped_refptr<CitizenNotesAgentHost> agent_host(CitizenNotesAgentHost::GetOrCreateFor(
      WebContents::FromRenderViewHost(inspected_rvh)));
  client_host.InspectAgentHost(agent_host.get());

  // Start a timeout.
  inspected_rvh->GetWidget()->StartInputEventAckTimeout();
  task_environment()->FastForwardBy(kHungRendererDelay +
                                    base::Milliseconds(10));
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close citizennotes and check that the notification is delivered.
  client_host.Close();
  // Start a timeout.
  inspected_rvh->GetWidget()->StartInputEventAckTimeout();
  task_environment()->FastForwardBy(kHungRendererDelay +
                                    base::Milliseconds(10));
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->SetDelegate(nullptr);
}

class TestExternalAgentDelegate : public CitizenNotesExternalAgentProxyDelegate {
 public:
  TestExternalAgentDelegate() {}
  ~TestExternalAgentDelegate() override {
    expectEvent(1, "Attach");
    expectEvent(1, "Detach");
    expectEvent(0, "SendMessageToBackend.message0");
    expectEvent(1, "SendMessageToBackend.message1");
    expectEvent(2, "SendMessageToBackend.message2");
  }

 private:
  std::map<std::string, int> event_counter_;

  void recordEvent(const std::string& name) {
    if (!base::Contains(event_counter_, name)) {
      event_counter_[name] = 0;
    }
    event_counter_[name] = event_counter_[name] + 1;
  }

  void expectEvent(int count, const std::string& name) {
    EXPECT_EQ(count, event_counter_[name]);
  }

  void Attach(CitizenNotesExternalAgentProxy* proxy) override {
    recordEvent("Attach");
  }

  void Detach(CitizenNotesExternalAgentProxy* proxy) override {
    recordEvent("Detach");
  }

  std::string GetType() override { return std::string(); }
  std::string GetTitle() override { return std::string(); }
  std::string GetDescription() override { return std::string(); }
  GURL GetURL() override { return GURL(); }
  GURL GetFaviconURL() override { return GURL(); }
  std::string GetFrontendURL() override { return std::string(); }
  bool Activate() override { return false; }
  void Reload() override {}
  bool Close() override { return false; }
  base::TimeTicks GetLastActivityTime() override { return base::TimeTicks(); }

  void SendMessageToBackend(CitizenNotesExternalAgentProxy* proxy,
                            base::span<const uint8_t> message) override {
    recordEvent(std::string("SendMessageToBackend.") +
                std::string(message.begin(), message.end()));
  }
};

TEST_F(CitizenNotesAgentHostImplTest, TestExternalProxy) {
  std::unique_ptr<TestExternalAgentDelegate> delegate(
      new TestExternalAgentDelegate());

  scoped_refptr<CitizenNotesAgentHost> agent_host = CitizenNotesAgentHost::Forward(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), std::move(delegate));
  EXPECT_EQ(agent_host, CitizenNotesAgentHost::GetForId(agent_host->GetId()));

  TestCitizenNotesClientHost client_host;
  client_host.InspectAgentHost(agent_host.get());

  agent_host->DispatchProtocolMessage(
      &client_host,
      base::as_bytes(base::make_span("message1", strlen("message1"))));
  agent_host->DispatchProtocolMessage(
      &client_host,
      base::as_bytes(base::make_span("message2", strlen("message2"))));
  agent_host->DispatchProtocolMessage(
      &client_host,
      base::as_bytes(base::make_span("message2", strlen("message2"))));

  client_host.Close();
}

}  // namespace content
