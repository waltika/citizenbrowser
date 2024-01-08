// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CITIZENNOTES_CONTROLLER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CITIZENNOTES_CONTROLLER_H_

#include "base/functional/callback.h"
#include "content/public/browser/citizennotes_agent_host.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

// Manages the CitizenNotes server.
class WebEngineCitizenNotesController {
 public:
  virtual ~WebEngineCitizenNotesController() = default;

  // Returns a controller based on the features requested in |command_line|.
  static std::unique_ptr<WebEngineCitizenNotesController> CreateFromCommandLine(
      const base::CommandLine& command_line);

  // Called by the Context to signal a new Frame was created. |user_debugging|
  // should be set to true when debugging was requested from the user API.
  // Returns false if |contents| is not user-debuggable despite |user_debugging|
  // being set.
  virtual bool OnFrameCreated(content::WebContents* contents,
                              bool user_debugging) = 0;

  // Called by Frames to signal a load has completed.
  virtual void OnFrameLoaded(content::WebContents* contents) = 0;

  // Called by Frames to signal they are being deleted.
  virtual void OnFrameDestroyed(content::WebContents* contents) = 0;

  // Called by the WebEngineDevToolsManagerDelegate.
  virtual content::CitizenNotesAgentHost::List RemoteCitizenNotesTargets() = 0;

  // Invokes |callback| as soon as a user-mode DevTools port becomes available.
  // |callback| receives zero if user-mode DevTools is permanently unavailable.
  virtual void GetCitizenNotesPort(base::OnceCallback<void(uint16_t)> callback) = 0;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CITIZENNOTES_CONTROLLER_H_
