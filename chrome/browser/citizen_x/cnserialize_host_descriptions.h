// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CITIZENNOTES_SERIALIZE_HOST_DESCRIPTIONS_H_
#define CHROME_BROWSER_CITIZENNOTES_SERIALIZE_HOST_DESCRIPTIONS_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"

// CitizenNotesAgentHost description to be serialized by CNSerializeHostDescriptions.
struct CNHostDescriptionNode {
  std::string name;
  std::string parent_name;
  base::Value representation;
};

// A helper function taking a CNHostDescriptionNode representation of hosts and
// producing a list of representations. The representation contains a list of
// dictionaries for each root in host, and has dictionaries of children
// injected into a list keyed |child_key| in the parent's dictionary.
base::Value::List CNSerializeHostDescriptions(
    std::vector<CNHostDescriptionNode> hosts,
    base::StringPiece child_key);

#endif  // CHROME_BROWSER_CITIZENNOTES_SERIALIZE_HOST_DESCRIPTIONS_H_
