// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_issue_storage.h"

#include "content/browser/citizen_x/protocol/audits.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace content {

static const unsigned kMaxIssueCount = 1000;

PAGE_USER_DATA_KEY_IMPL(CitizenNotesIssueStorage);

CitizenNotesIssueStorage::CitizenNotesIssueStorage(Page& page)
    : PageUserData<CitizenNotesIssueStorage>(page) {
  // CitizenNotesIssueStorage is only created for outermost pages.
  DCHECK(!page.GetMainDocument().GetParentOrOuterDocument());
}

CitizenNotesIssueStorage::~CitizenNotesIssueStorage() {
  // TOOD(1351587): remove explicit destructor once the bug is fixed.
  // This is so that crash key is scoped to issue destruction.
  SCOPED_CRASH_KEY_NUMBER("citizennotes", "audit_issue_count",
                          base::bits::Log2Floor(total_added_issues_));
  issues_.clear();
}

void CitizenNotesIssueStorage::AddInspectorIssue(
    RenderFrameHost* rfh,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK_LE(issues_.size(), kMaxIssueCount);
  if (issues_.size() == kMaxIssueCount) {
    issues_.pop_front();
  }
  total_added_issues_++;
  issues_.emplace_back(rfh->GetGlobalId(), std::move(issue));
}

std::vector<const protocol::Audits::InspectorIssue*>
CitizenNotesIssueStorage::FindIssuesForAgentOf(
    RenderFrameHost* render_frame_host) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderFrameHostImpl* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  RenderFrameHostImpl* main_rfh =
      static_cast<RenderFrameHostImpl*>(&page().GetMainDocument());
  CitizenNotesAgentHostImpl* agent_host =
      RenderFrameCitizenNotesAgentHost::GetFor(render_frame_host_impl);
  DCHECK_EQ(&render_frame_host->GetOutermostMainFrame()->GetPage(), &page());
  DCHECK(RenderFrameCitizenNotesAgentHost::ShouldCreateCitizenNotesForHost(
      render_frame_host_impl));
  DCHECK(agent_host);
  bool is_main_agent = render_frame_host_impl == main_rfh;

  std::vector<const protocol::Audits::InspectorIssue*> issues;
  for (const auto& entry : issues_) {
    bool should_add;
    RenderFrameHostImpl* issue_rfh = RenderFrameHostImpl::FromID(entry.first);
    if (!issue_rfh) {
      // Issues that fall in this category are either associated with |main_rfh|
      // or with deleted subframe RFHs of |main_rfh|. In both cases, we only
      // want to retrieve them for |main_rfh|'s agent.
      // Note: This means that issues for deleted subframe RFHs get reparented
      // to |main_rfh| after deletion.
      should_add = is_main_agent;
    } else {
      should_add =
          RenderFrameCitizenNotesAgentHost::GetFor(issue_rfh) == agent_host;
    }
    if (should_add)
      issues.push_back(entry.second.get());
  }
  return issues;
}

}  // namespace content
