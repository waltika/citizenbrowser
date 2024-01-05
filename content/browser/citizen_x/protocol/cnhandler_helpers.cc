// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/protocol/cnhandler_helpers.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"

namespace content {

namespace protocol {

FrameTreeNode* FrameTreeNodeFromCitizenNotesFrameToken(
    FrameTreeNode* root,
    const std::string& citizennotes_frame_token) {
  if (root->current_frame_host()->citizennotes_frame_token().ToString() ==
      citizennotes_frame_token) {
    return root;
  } else {
    for (FrameTreeNode* node : root->frame_tree().SubtreeNodes(root)) {
      if (node->current_frame_host()->citizennotes_frame_token().ToString() ==
          citizennotes_frame_token) {
        return node;
      }
    }
  }
  return nullptr;
}

}  // namespace protocol
}  // namespace content
