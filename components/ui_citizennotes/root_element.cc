// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_citizennotes/root_element.h"

#include "base/notreached.h"
#include "components/ui_citizennotes/protocol.h"
#include "components/ui_citizennotes/ui_element_delegate.h"

namespace ui_citizennotes {

RootElement::RootElement(UIElementDelegate* ui_element_delegate)
    : UIElement(UIElementType::ROOT, ui_element_delegate, nullptr) {}

RootElement::~RootElement() {}

void RootElement::GetBounds(gfx::Rect* bounds) const {
  NOTREACHED();
}

void RootElement::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void RootElement::GetVisible(bool* visible) const {
  NOTREACHED();
}

void RootElement::SetVisible(bool visible) {
  NOTREACHED();
}

std::vector<std::string> RootElement::GetAttributes() const {
  NOTREACHED();
  return {};
}

std::pair<gfx::NativeWindow, gfx::Rect>
RootElement::GetNodeWindowAndScreenBounds() const {
  NOTREACHED();
  return {};
}

}  // namespace ui_citizennotes
