// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_frontend_host_impl.h"

#include <stddef.h>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/webui/resource_path.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"  // nogncheck
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"  // nogncheck
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

extern const webui::ResourcePath kCitizennotesResources[];
extern const size_t kCitizennotesResourcesSize;

namespace content {
namespace {
const char kCompatibilityScript[] = "citizennotes_compatibility.js";
const char kCompatibilityScriptSourceURL[] =
    "\n//# "
    "sourceURL=citizennotes://citizennotes/bundled/citizennotes_compatibility.js";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Remove the pieces of the URL we don't want to send back with the error
// reports. In particular, do not send query or fragments as those can have
// privacy-sensitive information in them.
std::string RedactURL(const GURL& url) {
  std::string redacted_url = url.DeprecatedGetOriginAsURL().spec();
  // Path will start with / and GetOrigin ends with /. Cut one / to avoid
  // chrome://discards//graph.
  if (!redacted_url.empty() && redacted_url.back() == '/') {
    redacted_url.pop_back();
  }
  base::StrAppend(&redacted_url, {url.path_piece()});
  return redacted_url;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

// static
std::unique_ptr<CitizenNotesFrontendHost> CitizenNotesFrontendHost::Create(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback) {
  DCHECK(!frame_host->GetParent());
  return std::make_unique<CitizenNotesFrontendHostImpl>(frame_host,
                                                    handle_message_callback);
}

// static
std::unique_ptr<CitizenNotesFrontendHostImpl>
CitizenNotesFrontendHostImpl::CreateForTesting(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback) {
  DCHECK(!frame_host->GetParent());
  return std::make_unique<CitizenNotesFrontendHostImpl>(frame_host,
                                                    handle_message_callback);
}

// static
void CitizenNotesFrontendHost::SetupExtensionsAPI(
    RenderFrameHost* frame_host,
    const std::string& extension_api) {
  DCHECK(frame_host->GetParent());
  mojo::AssociatedRemote<blink::mojom::CitizenNotesFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  frontend->SetupCitizenNotesExtensionAPI(extension_api);
}

// static
scoped_refptr<base::RefCountedMemory>
CitizenNotesFrontendHost::GetFrontendResourceBytes(const std::string& path) {
  for (size_t i = 0; i < kCitizennotesResourcesSize; ++i) {
    if (path == kCitizennotesResources[i].path) {
      return GetContentClient()->GetDataResourceBytes(kCitizennotesResources[i].id);
    }
  }
  return nullptr;
}

// static
std::string CitizenNotesFrontendHost::GetFrontendResource(const std::string& path) {
  scoped_refptr<base::RefCountedMemory> bytes = GetFrontendResourceBytes(path);
  if (!bytes)
    return std::string();
  return std::string(bytes->front_as<char>(), bytes->size());
}

CitizenNotesFrontendHostImpl::CitizenNotesFrontendHostImpl(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback)
    : web_contents_(WebContents::FromRenderFrameHost(frame_host)),
      handle_message_callback_(handle_message_callback) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  Observe(web_contents_);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojo::AssociatedRemote<blink::mojom::CitizenNotesFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  std::string api_script =
      content::CitizenNotesFrontendHost::GetFrontendResource(kCompatibilityScript) +
      kCompatibilityScriptSourceURL;
  frontend->SetupCitizenNotesFrontend(api_script,
                                  receiver_.BindNewEndpointAndPassRemote());
}

CitizenNotesFrontendHostImpl::~CitizenNotesFrontendHostImpl() = default;

void CitizenNotesFrontendHostImpl::BadMessageReceived() {
  bad_message::ReceivedBadMessage(
      web_contents_->GetPrimaryMainFrame()->GetProcess(),
      bad_message::DFH_BAD_EMBEDDER_MESSAGE);
}

void CitizenNotesFrontendHostImpl::DispatchEmbedderMessage(
    base::Value::Dict message) {
  handle_message_callback_.Run(std::move(message));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void CitizenNotesFrontendHostImpl::OnDidAddMessageToConsole(
    RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const absl::optional<std::u16string>& untrusted_stack_trace) {
  if (!base::FeatureList::IsEnabled(
          features::kEnableCitizenNotesJsErrorReporting)) {
    return;
  }

  DVLOG(3) << "OnDidAddMessageToConsole called for " << message;

  if (log_level != blink::mojom::ConsoleMessageLevel::kError) {
    DVLOG(3) << "Message not reported, not an error";
    return;
  }

  scoped_refptr<JsErrorReportProcessor> processor =
      JsErrorReportProcessor::Get();
  if (!processor) {
    // This usually means we are not on an official Google build, FYI.
    DVLOG(3) << "Message not reported, no processor";
    return;
  }

  GURL url(source_id);
  if (!url.is_valid()) {
    DVLOG(3) << "Message not reported, invalid URL: " << source_id;
    return;
  }

  // If this is not a citizennotes:// page, do not report the error
  // This allows us to filter out messages from web page and extensions.
  // Even extensions are part of the CitizenNotes (via
  // chrome.citizennotes.panels.create) the source for logs from them starts with
  // `chrome-extension://`
  if (!url.SchemeIs(kChromeCitizenNotesScheme)) {
    DVLOG(3) << "Message not reported, not a citizennotes:// URL";
    return;
  }

  JavaScriptErrorReport report;
  report.message = base::UTF16ToUTF8(message);
  report.line_number = line_no;
  report.url = RedactURL(url);
  report.source_system = JavaScriptErrorReport::SourceSystem::kCitizenNotesObserver;
  if (untrusted_stack_trace) {
    report.stack_trace = base::UTF16ToUTF8(*untrusted_stack_trace);
  }

  processor->SendErrorReport(std::move(report), base::DoNothing(),
                             web_contents()->GetBrowserContext());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace content
