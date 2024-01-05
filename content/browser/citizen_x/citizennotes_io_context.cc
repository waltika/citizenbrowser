// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/citizennotes_io_context.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/citizen_x/citizennotes_stream_blob.h"
#include "content/browser/citizen_x/citizennotes_stream_file.h"

namespace content {

CitizenNotesIOContext::Stream::Stream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RefCountedDeleteOnSequence<CitizenNotesIOContext::Stream>(
          std::move(task_runner)) {}

std::string CitizenNotesIOContext::Stream::Register(CitizenNotesIOContext* context) {
  static unsigned s_last_stream_handle = 0;
  const std::string handle = base::NumberToString(++s_last_stream_handle);
  Register(context, handle);
  return handle;
}

void CitizenNotesIOContext::Stream::Register(CitizenNotesIOContext* context,
                                         const std::string& handle) {
  context->RegisterStream(this, handle);
}

bool CitizenNotesIOContext::Stream::SupportsSeek() const {
  return true;
}

CitizenNotesIOContext::Stream::~Stream() = default;

CitizenNotesIOContext::CitizenNotesIOContext() = default;

CitizenNotesIOContext::~CitizenNotesIOContext() = default;

void CitizenNotesIOContext::RegisterStream(scoped_refptr<Stream> stream,
                                       const std::string& id) {
  bool inserted = streams_.emplace(id, std::move(stream)).second;
  DCHECK(inserted);
}

scoped_refptr<CitizenNotesIOContext::Stream> CitizenNotesIOContext::GetByHandle(
    const std::string& handle) {
  auto it = streams_.find(handle);
  return it == streams_.end() ? nullptr : it->second;
}

bool CitizenNotesIOContext::Close(const std::string& handle) {
  size_t erased_count = streams_.erase(handle);
  return !!erased_count;
}

void CitizenNotesIOContext::DiscardAllStreams() {
  streams_.clear();
}

// static
bool CitizenNotesIOContext::IsTextMimeType(const std::string& mime_type) {
  static const char* kTextMIMETypePrefixes[] = {
      "text/", "application/x-javascript", "application/json",
      "application/xml"};
  for (size_t i = 0; i < std::size(kTextMIMETypePrefixes); ++i) {
    if (base::StartsWith(mime_type, kTextMIMETypePrefixes[i],
                         base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }
  return false;
}

}  // namespace content
