// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_CSS_AGENT_H_
#define COMPONENTS_UI_CITIZENNOTES_CSS_AGENT_H_

#include "components/ui_citizennotes/css.h"
#include "components/ui_citizennotes/dom_agent.h"

namespace gfx {
class Rect;
}

namespace ui_citizennotes {

class UIElement;

class UI_CITIZENNOTES_EXPORT CSSAgent
    : public UiCitizenNotesBaseAgent<protocol::CSS::Metainfo>,
      public DOMAgentObserver {
 public:
  explicit CSSAgent(DOMAgent* dom_agent);

  CSSAgent(const CSSAgent&) = delete;
  CSSAgent& operator=(const CSSAgent&) = delete;

  ~CSSAgent() override;

  // CSS::Backend:
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getMatchedStylesForNode(
      int node_id,
      protocol::Maybe<protocol::Array<protocol::CSS::RuleMatch>>*
          matched_css_rules) override;
  protocol::Response getStyleSheetText(const protocol::String& style_sheet_id,
                                       protocol::String* text) override;
  protocol::Response setStyleTexts(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>
          edits,
      std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* result)
      override;

  // DOMAgentObserver:
  void OnElementBoundsChanged(UIElement* ui_element) override;

 private:
  std::unique_ptr<protocol::CSS::CSSStyle> GetStylesForUIElement(
      UIElement* ui_element);
  void InvalidateStyleSheet(UIElement* ui_element);
  bool GetPropertiesForUIElement(UIElement* ui_element,
                                 gfx::Rect* bounds,
                                 bool* visible);
  bool SetPropertiesForUIElement(UIElement* ui_element,
                                 const gfx::Rect& bounds,
                                 bool visible);
  std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>> BuildMatchedStyles(
      UIElement* ui_element);

  // Sends header to frontend for each section in properties panel.
  void InitStylesheetHeaders(UIElement* ui_element);
  
  RAW_PTR_EXCLUSION DOMAgent* const dom_agent_;
};

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_CSS_AGENT_H_
