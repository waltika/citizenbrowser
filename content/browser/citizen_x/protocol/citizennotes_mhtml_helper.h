// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_MHTML_HELPER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_MHTML_HELPER_H_

#include "content/browser/citizen_x/protocol/cnpage_handler.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace content {
namespace protocol {

class CitizenNotesMHTMLHelper
    : public base::RefCountedThreadSafe<CitizenNotesMHTMLHelper> {
 public:
  static void Capture(
      const WebContents::Getter& web_contents_getter,
      std::unique_ptr<CNPageHandler::CaptureSnapshotCallback> callback);

 private:
  CitizenNotesMHTMLHelper(
      const WebContents::Getter& web_contents_getter,
      std::unique_ptr<CNPageHandler::CaptureSnapshotCallback> callback);
  ~CitizenNotesMHTMLHelper();

  void CreateTemporaryFile();
  void TemporaryFileCreatedOnIO();
  void TemporaryFileCreatedOnUI();
  void MHTMLGeneratedOnUI(int64_t mhtml_file_size);
  void ReadMHTML();
  void ReportFailure(const std::string& message);
  void ReportSuccess(std::unique_ptr<std::string> mhtml_snapshot);

  WebContents::Getter web_contents_getter_;
  std::unique_ptr<CNPageHandler::CaptureSnapshotCallback> callback_;
  scoped_refptr<storage::ShareableFileReference> mhtml_file_;
  base::FilePath mhtml_snapshot_path_;

  friend class base::RefCountedThreadSafe<CitizenNotesMHTMLHelper>;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_MHTML_HELPER_H_
