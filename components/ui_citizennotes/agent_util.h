// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_CITIZENNOTES_AGENT_UTIL_H_
#define COMPONENTS_UI_CITIZENNOTES_AGENT_UTIL_H_

#include <string>

#include "components/ui_citizennotes/citizennotes_export.h"

namespace ui_citizennotes {

UI_CITIZENNOTES_EXPORT extern const char kChromiumCodeSearchURL[];
UI_CITIZENNOTES_EXPORT extern const char kChromiumCodeSearchSrcURL[];

// Synchonously gets source code and returns true if successful.
bool UI_CITIZENNOTES_EXPORT GetSourceCode(std::string path,
                                      std::string* source_code);

}  // namespace ui_citizennotes

#endif  // COMPONENTS_UI_CITIZENNOTES_AGENT_UTIL_H_
