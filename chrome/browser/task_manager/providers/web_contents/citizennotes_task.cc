// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/citizennotes_task.h"

#include "content/public/browser/web_contents.h"

namespace task_manager {

CitizenNotesTask::CitizenNotesTask(content::WebContents* web_contents)
    : TabContentsTask(web_contents) {
}

CitizenNotesTask::~CitizenNotesTask() {
}

}  // namespace task_manager
