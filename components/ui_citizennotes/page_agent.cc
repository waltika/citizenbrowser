// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_citizennotes/page_agent.h"

namespace ui_citizennotes {

PageAgent::PageAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {}

PageAgent::~PageAgent() {}

protocol::Response PageAgent::reload(protocol::Maybe<bool> bypass_cache) {
  NOTREACHED();
  return protocol::Response::Success();
}

}  // namespace ui_citizennotes
