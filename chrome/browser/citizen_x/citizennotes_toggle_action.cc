// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/citizen_x/citizennotes_toggle_action.h"

#include <memory>

CitizenNotesToggleAction::RevealParams::RevealParams(const std::u16string& url,
                                                 size_t line_number,
                                                 size_t column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

CitizenNotesToggleAction::RevealParams::~RevealParams() {
}

CitizenNotesToggleAction::CitizenNotesToggleAction(Type type) : type_(type) {
}

CitizenNotesToggleAction::CitizenNotesToggleAction(RevealParams* params)
    : type_(kReveal), params_(params) {
}

CitizenNotesToggleAction::CitizenNotesToggleAction(const CitizenNotesToggleAction& rhs)
    : type_(rhs.type_),
      params_(rhs.params_.get() ? new RevealParams(*rhs.params_) : nullptr) {}

void CitizenNotesToggleAction::operator=(const CitizenNotesToggleAction& rhs) {
  type_ = rhs.type_;
  if (rhs.params_.get())
    params_ = std::make_unique<RevealParams>(*rhs.params_);
}

CitizenNotesToggleAction::~CitizenNotesToggleAction() {
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::Show() {
  return CitizenNotesToggleAction(kShow);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::ShowConsolePanel() {
  return CitizenNotesToggleAction(kShowConsolePanel);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::ShowElementsPanel() {
  return CitizenNotesToggleAction(kShowElementsPanel);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::PauseInDebugger() {
  return CitizenNotesToggleAction(kPauseInDebugger);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::Inspect() {
  return CitizenNotesToggleAction(kInspect);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::Toggle() {
  return CitizenNotesToggleAction(kToggle);
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::Reveal(const std::u16string& url,
                                                  size_t line_number,
                                                  size_t column_number) {
  return CitizenNotesToggleAction(
      new RevealParams(url, line_number, column_number));
}

// static
CitizenNotesToggleAction CitizenNotesToggleAction::NoOp() {
  return CitizenNotesToggleAction(kNoOp);
}
