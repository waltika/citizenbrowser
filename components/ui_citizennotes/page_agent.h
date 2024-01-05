// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_PAGE_AGENT_H_
#define COMPONENTS_UI_CITIZENNOTES_PAGE_AGENT_H_

#include "components/ui_citizennotes/dom_agent.h"
#include "components/ui_citizennotes/page.h"

namespace ui_citizennotes {

class UI_CITIZENNOTES_EXPORT PageAgent
    : public UiCitizenNotesBaseAgent<protocol::Page::Metainfo> {
 public:
  explicit PageAgent(DOMAgent* dom_agent);

  PageAgent(const PageAgent&) = delete;
  PageAgent& operator=(const PageAgent&) = delete;

  ~PageAgent() override;

  // Called on Ctrl+R (windows, linux) or Meta+R (mac) from frontend, but used
  // in UI Citizennotes to toggle the bounds debug rectangles for views. If called
  // using Ctrl+Shift+R (windows, linux) or Meta+Shift+R (mac), |bypass_cache|
  // will be true and will toggle bubble lock.
  protocol::Response reload(protocol::Maybe<bool> bypass_cache) override;

 protected:
   RAW_PTR_EXCLUSION DOMAgent* const dom_agent_;
};

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_PAGE_AGENT_H_
