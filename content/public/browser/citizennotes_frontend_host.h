// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CITIZENNOTES_FRONTEND_HOST_H_
#define CONTENT_PUBLIC_BROWSER_CITIZENNOTES_FRONTEND_HOST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace base {
class RefCountedMemory;
}

namespace content {

class RenderFrameHost;

// This class dispatches messages between CitizenNotes frontend and Delegate
// which is implemented by the embedder.
// This allows us to avoid exposing CitizenNotes frontend messages through
// the content public API.
// Note: CitizenNotesFrontendHost is not supported on Android.
class CitizenNotesFrontendHost {
 public:
  using HandleMessageCallback =
      base::RepeatingCallback<void(base::Value::Dict)>;

  // Creates a new CitizenNotesFrontendHost for RenderFrameHost where CitizenNotes
  // frontend is loaded.
  CONTENT_EXPORT static std::unique_ptr<CitizenNotesFrontendHost> Create(
      RenderFrameHost* frontend_main_frame,
      const HandleMessageCallback& handle_message_callback);

  CONTENT_EXPORT static void SetupExtensionsAPI(
      RenderFrameHost* frame,
      const std::string& extension_api);

  CONTENT_EXPORT virtual ~CitizenNotesFrontendHost() {}

  CONTENT_EXPORT virtual void BadMessageReceived() {}

  // Returns bundled CitizenNotes frontend resource by |path|. Returns null if
  // |path| does not correspond to any frontend resource.
  CONTENT_EXPORT static scoped_refptr<base::RefCountedMemory>
  GetFrontendResourceBytes(const std::string& path);

  // Convenience wrapper to return GetFrontendResourceBytes() as a string.
  CONTENT_EXPORT static std::string GetFrontendResource(
      const std::string& path);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CITIZENNOTES_FRONTEND_HOST_H_
