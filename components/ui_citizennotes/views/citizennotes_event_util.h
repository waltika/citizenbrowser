// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_EVENT_UTIL_H_
#define COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_EVENT_UTIL_H_

#include "components/ui_citizennotes/dom.h"
#include "ui/events/event_utils.h"

namespace ui_citizennotes {

ui::KeyEvent ConvertToUIKeyEvent(protocol::DOM::KeyEvent* event);

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_VIEWS_CITIZENNOTES_EVENT_UTIL_H_
