// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_OVERLAY_AGENT_H_
#define COMPONENTS_UI_CITIZENNOTES_OVERLAY_AGENT_H_

#include "components/ui_citizennotes/dom_agent.h"
#include "components/ui_citizennotes/overlay.h"

namespace ui_citizennotes {

class UI_CITIZENNOTES_EXPORT OverlayAgent
    : public UiCitizenNotesBaseAgent<protocol::Overlay::Metainfo> {
 public:
  explicit OverlayAgent(DOMAgent* dom_agent);

  OverlayAgent(const OverlayAgent&) = delete;
  OverlayAgent& operator=(const OverlayAgent&) = delete;

  ~OverlayAgent() override;

  // Overlay::Backend:
  protocol::Response setInspectMode(
      const protocol::String& in_mode,
      protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig)
      override;
  protocol::Response highlightNode(
      std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
      protocol::Maybe<int> node_id) override;
  protocol::Response hideHighlight() override;

 protected:
  DOMAgent* dom_agent() const { return dom_agent_; }

 private:
  RAW_PTR_EXCLUSION DOMAgent* const dom_agent_;
};

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_OVERLAY_AGENT_H_
