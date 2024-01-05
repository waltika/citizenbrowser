// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/citizennotes_manager_delegate.h"
#include "base/values.h"
#include "content/public/browser/citizennotes_agent_host.h"

namespace content {

void CitizenNotesManagerDelegate::Inspect(CitizenNotesAgentHost* agent_host) {
}

std::string CitizenNotesManagerDelegate::GetTargetType(WebContents* wc) {
  return std::string();
}

std::string CitizenNotesManagerDelegate::GetTargetTitle(WebContents* wc) {
  return std::string();
}

std::string CitizenNotesManagerDelegate::GetTargetDescription(WebContents* wc) {
  return std::string();
}

bool CitizenNotesManagerDelegate::AllowInspectingRenderFrameHost(
    RenderFrameHost* rfh) {
  return true;
}

CitizenNotesAgentHost::List CitizenNotesManagerDelegate::RemoteDebuggingTargets(
    CitizenNotesManagerDelegate::TargetType target_type) {
  return CitizenNotesAgentHost::GetOrCreateAll();
}

scoped_refptr<CitizenNotesAgentHost> CitizenNotesManagerDelegate::CreateNewTarget(
    const GURL& url,
    CitizenNotesManagerDelegate::TargetType target_type) {
  return nullptr;
}

std::vector<BrowserContext*> CitizenNotesManagerDelegate::GetBrowserContexts() {
  return std::vector<BrowserContext*>();
}

BrowserContext* CitizenNotesManagerDelegate::GetDefaultBrowserContext() {
  return nullptr;
}

BrowserContext* CitizenNotesManagerDelegate::CreateBrowserContext() {
  return nullptr;
}

void CitizenNotesManagerDelegate::DisposeBrowserContext(BrowserContext*,
                                                    DisposeCallback callback) {
  std::move(callback).Run(false, "Browser Context disposal is not supported");
}

void CitizenNotesManagerDelegate::ClientAttached(
    CitizenNotesAgentHostClientChannel* channel) {}
void CitizenNotesManagerDelegate::ClientDetached(
    CitizenNotesAgentHostClientChannel* channel) {}

void CitizenNotesManagerDelegate::HandleCommand(
    CitizenNotesAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  std::move(callback).Run(message);
}

std::string CitizenNotesManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

bool CitizenNotesManagerDelegate::HasBundledFrontendResources() {
  return false;
}

bool CitizenNotesManagerDelegate::IsBrowserTargetDiscoverable() {
  return false;
}

CitizenNotesManagerDelegate::~CitizenNotesManagerDelegate() {
}

}  // namespace content
