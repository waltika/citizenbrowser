// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CITIZENNOTES_CITIZENNOTES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CITIZENNOTES_CITIZENNOTES_UI_H_

#include "chrome/browser/citizen_x/citizennotes_ui_bindings.h"
#include "content/public/browser/web_ui_controller.h"

class CitizenNotesUI : public content::WebUIController {
 public:
  static GURL GetProxyURL(const std::string& frontend_url);
  static GURL GetRemoteBaseURL();
  static bool IsFrontendResourceURL(const GURL& url);

  explicit CitizenNotesUI(content::WebUI* web_ui);

  CitizenNotesUI(const CitizenNotesUI&) = delete;
  CitizenNotesUI& operator=(const CitizenNotesUI&) = delete;

  ~CitizenNotesUI() override;

 private:
  CitizenNotesUIBindings bindings_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CITIZENNOTES_CITIZENNOTES_UI_H_
