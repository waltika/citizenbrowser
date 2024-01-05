// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_FRONTEND_HOST_IMPL_H_
#define CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_FRONTEND_HOST_IMPL_H_

#include "content/public/browser/citizennotes_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/citizennotes/citizennotes_frontend.mojom.h"

namespace content {

class WebContents;

class CitizenNotesFrontendHostImpl : public CitizenNotesFrontendHost,
                                 public blink::mojom::CitizenNotesFrontendHost,
                                 public WebContentsObserver {
 public:
  CitizenNotesFrontendHostImpl(
      RenderFrameHost* frame_host,
      const HandleMessageCallback& handle_message_callback);

  CitizenNotesFrontendHostImpl(const CitizenNotesFrontendHostImpl&) = delete;
  CitizenNotesFrontendHostImpl& operator=(const CitizenNotesFrontendHostImpl&) = delete;

  ~CitizenNotesFrontendHostImpl() override;

  static CONTENT_EXPORT std::unique_ptr<CitizenNotesFrontendHostImpl>
  CreateForTesting(RenderFrameHost* frame_host,
                   const HandleMessageCallback& handle_message_callback);

  void BadMessageReceived() override;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const absl::optional<std::u16string>& untrusted_stack_trace) override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

 private:
  // blink::mojom::CitizenNotesFrontendHost implementation.
  void DispatchEmbedderMessage(base::Value::Dict message) override;

  RAW_PTR_EXCLUSION WebContents* web_contents_;
  HandleMessageCallback handle_message_callback_;
  mojo::AssociatedReceiver<blink::mojom::CitizenNotesFrontendHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_CITIZENNOTES_FRONTEND_HOST_IMPL_H_
