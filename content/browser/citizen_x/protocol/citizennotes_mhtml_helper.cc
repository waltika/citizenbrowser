// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/citizennotes_mhtml_helper.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/mhtml_generation_params.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace content {
namespace protocol {

namespace {

void ClearFileReferenceOnIOThread(
    scoped_refptr<storage::ShareableFileReference>) {}

}  // namespace

CitizenNotesMHTMLHelper::CitizenNotesMHTMLHelper(
    const WebContents::Getter& web_contents_getter,
    std::unique_ptr<CNPageHandler::CaptureSnapshotCallback> callback)
    : web_contents_getter_(web_contents_getter),
      callback_(std::move(callback)) {}

CitizenNotesMHTMLHelper::~CitizenNotesMHTMLHelper() {
  if (mhtml_file_.get()) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ClearFileReferenceOnIOThread, std::move(mhtml_file_)));
  }
}

// static
void CitizenNotesMHTMLHelper::Capture(
    const WebContents::Getter& web_contents_getter,
    std::unique_ptr<CNPageHandler::CaptureSnapshotCallback> callback) {
  scoped_refptr<CitizenNotesMHTMLHelper> helper =
      new CitizenNotesMHTMLHelper(web_contents_getter, std::move(callback));
  base::ThreadPool::PostTask(
      FROM_HERE,
      {// Requires IO.
       base::MayBlock(),

       // TaskShutdownBehavior: use SKIP_ON_SHUTDOWN so that the helper's
       // fields do not suddenly become invalid.
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CitizenNotesMHTMLHelper::CreateTemporaryFile, helper));
}

void CitizenNotesMHTMLHelper::CreateTemporaryFile() {
  if (!base::CreateTemporaryFile(&mhtml_snapshot_path_)) {
    ReportFailure("Unable to create temporary file");
    return;
  }
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CitizenNotesMHTMLHelper::TemporaryFileCreatedOnIO, this));
}

void CitizenNotesMHTMLHelper::TemporaryFileCreatedOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Setup a ShareableFileReference so the temporary file gets deleted
  // once it is no longer used.
  mhtml_file_ = storage::ShareableFileReference::GetOrCreate(
      mhtml_snapshot_path_,
      storage::ShareableFileReference::DELETE_ON_FINAL_RELEASE,
      base::ThreadPool::CreateSequencedTaskRunner(
          {// Requires IO.
           base::MayBlock(),

           // Because we are using DELETE_ON_FINAL_RELEASE here, the
           // storage::ScopedFile inside ShareableFileReference requires
           // a shutdown blocking task runner to ensure that its deletion
           // task runs.
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
          .get());

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CitizenNotesMHTMLHelper::TemporaryFileCreatedOnUI, this));
}

void CitizenNotesMHTMLHelper::TemporaryFileCreatedOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = web_contents_getter_.Run();
  if (!web_contents) {
    ReportFailure("No web contents");
    return;
  }
  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(mhtml_snapshot_path_),
      base::BindOnce(&CitizenNotesMHTMLHelper::MHTMLGeneratedOnUI, this));
}

void CitizenNotesMHTMLHelper::MHTMLGeneratedOnUI(int64_t mhtml_file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (mhtml_file_size <= 0 ||
      mhtml_file_size > std::numeric_limits<int>::max()) {
    ReportFailure("Failed to generate MHTML");
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      {// Requires IO.
       base::MayBlock(),

       // TaskShutdownBehavior: use SKIP_ON_SHUTDOWN so that the
       // helper's fields do not suddenly become invalid.
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CitizenNotesMHTMLHelper::ReadMHTML, this));
}

void CitizenNotesMHTMLHelper::ReadMHTML() {
  std::string buffer;
  if (!base::ReadFileToString(mhtml_snapshot_path_, &buffer)) {
    LOG(ERROR) << "Failed to read " << mhtml_snapshot_path_;
    ReportFailure("Failed to read MHTML file");
    return;
  }
  std::unique_ptr<std::string> buffer_ptr =
      std::make_unique<std::string>(buffer);
  ReportSuccess(std::move(buffer_ptr));
}

void CitizenNotesMHTMLHelper::ReportFailure(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CitizenNotesMHTMLHelper::ReportFailure, this, message));
    return;
  }
  if (message.empty())
    callback_->sendFailure(Response::InternalError());
  else
    callback_->sendFailure(Response::ServerError(message));
}

void CitizenNotesMHTMLHelper::ReportSuccess(
    std::unique_ptr<std::string> mhtml_snapshot) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CitizenNotesMHTMLHelper::ReportSuccess, this,
                                  std::move(mhtml_snapshot)));
    return;
  }
  callback_->sendSuccess(*mhtml_snapshot);
}

}  // namespace protocol
}  // namespace content
