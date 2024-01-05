// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_CITIZENNOTES_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_CITIZENNOTES_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/tab_contents_task.h"

namespace task_manager {

// Defines a task manager representation of the developer tools WebContents.
class CitizenNotesTask : public TabContentsTask {
 public:
  explicit CitizenNotesTask(content::WebContents* web_contents);
  CitizenNotesTask(const CitizenNotesTask&) = delete;
  CitizenNotesTask& operator=(const CitizenNotesTask&) = delete;
  ~CitizenNotesTask() override;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_CITIZENNOTES_TASK_H_
