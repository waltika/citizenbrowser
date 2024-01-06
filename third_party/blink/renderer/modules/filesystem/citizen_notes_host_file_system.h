// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_CITIZEN_NOTES_HOST_FILE_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_CITIZEN_NOTES_HOST_FILE_SYSTEM_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMFileSystem;
class CitizenNotesHost;

class CitizenNotesHostFileSystem {
  STATIC_ONLY(CitizenNotesHostFileSystem);

 public:
  static DOMFileSystem* isolatedFileSystem(CitizenNotesHost&,
                                           const String& file_system_name,
                                           const String& root_url);
  static void upgradeDraggedFileSystemPermissions(CitizenNotesHost&,
                                                  DOMFileSystem*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_CITIZEN_NOTES_HOST_FILE_SYSTEM_H_
