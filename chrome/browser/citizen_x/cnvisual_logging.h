// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_VISUAL_LOGGING_H_
#define CHROME_BROWSER_CITIZENNOTES_VISUAL_LOGGING_H_

#include <vector>

struct CNVisualElementImpression {
  int id = -1;
  int type = -1;
  int parent = -1;
  int context = -1;
};

struct CNImpressionEvent {
  CNImpressionEvent();
  ~CNImpressionEvent();
  std::vector<CNVisualElementImpression> impressions;
};

struct CNClickEvent {
  int veid = -1;
  int mouse_button = -1;
  int context = -1;
};

struct CNHoverEvent {
  int veid = -1;
  int time = -1;
  int context = -1;
};

struct CNDragEvent {
  int veid = -1;
  int distance = -1;
  int context = -1;
};

struct CNChangeEvent {
  int veid = -1;
  int context = -1;
};

struct CNKeyDownEvent {
  int veid = -1;
  int context = -1;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_VISUAL_LOGGING_H_
