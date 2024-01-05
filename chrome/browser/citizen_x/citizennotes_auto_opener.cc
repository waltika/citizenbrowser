// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizennotes_auto_opener.h"

#include "base/command_line.h"
#include "chrome/browser/citizen_x/citizennotes_window.h"

CitizenNotesAutoOpener::CitizenNotesAutoOpener()
    : browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
}

CitizenNotesAutoOpener::~CitizenNotesAutoOpener() {
}

void CitizenNotesAutoOpener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;

  for (const auto& contents : change.GetInsert()->contents) {
    if (!CitizenNotesWindow::IsCitizenNotesWindow(contents.contents)) {
      CitizenNotesWindow::OpenCitizenNotesWindow(
          contents.contents, CitizenNotesOpenedByAction::kAutomaticForNewTarget);
    }
  }
}
