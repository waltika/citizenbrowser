// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TOGGLE_ACTION_H_
#define CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TOGGLE_ACTION_H_

#include <stddef.h>

#include <memory>
#include <string>


struct CitizenNotesToggleAction {
 public:
  enum Type {
    kShow,
    kShowConsolePanel,
    kShowElementsPanel,
    kPauseInDebugger,
    kInspect,
    kToggle,
    kReveal,
    kNoOp
  };

  struct RevealParams {
    RevealParams(const std::u16string& url,
                 size_t line_number,
                 size_t column_number);
    ~RevealParams();

    std::u16string url;
    size_t line_number;
    size_t column_number;
  };

  void operator=(const CitizenNotesToggleAction& rhs);
  CitizenNotesToggleAction(const CitizenNotesToggleAction& rhs);
  ~CitizenNotesToggleAction();

  static CitizenNotesToggleAction Show();
  static CitizenNotesToggleAction ShowConsolePanel();
  static CitizenNotesToggleAction ShowElementsPanel();
  static CitizenNotesToggleAction PauseInDebugger();
  static CitizenNotesToggleAction Inspect();
  static CitizenNotesToggleAction Toggle();
  static CitizenNotesToggleAction Reveal(const std::u16string& url,
                                     size_t line_number,
                                     size_t column_number);
  static CitizenNotesToggleAction NoOp();

  Type type() const { return type_; }
  const RevealParams* params() const { return params_.get(); }

 private:
  explicit CitizenNotesToggleAction(Type type);
  explicit CitizenNotesToggleAction(RevealParams* reveal_params);

  // The type of action.
  Type type_;

  // Additional parameters for the Reveal action; NULL if of any other type.
  std::unique_ptr<RevealParams> params_;
};

#endif  // CHROME_BROWSER_CITIZENNOTES_CITIZENNOTES_TOGGLE_ACTION_H_
