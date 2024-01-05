// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_SCHEMA_HANDLER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_SCHEMA_HANDLER_H_

#include "content/browser/citizen_x/protocol/citizennotes_domain_handler.h"
#include "content/browser/citizen_x/protocol/schema.h"

namespace content {
namespace protocol {

class CNSchemaHandler : public CitizenNotesDomainHandler,
                      public Schema::Backend {
 public:
  CNSchemaHandler();

  CNSchemaHandler(const CNSchemaHandler&) = delete;
  CNSchemaHandler& operator=(const CNSchemaHandler&) = delete;

  ~CNSchemaHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  Response GetDomains(
      std::unique_ptr<protocol::Array<Schema::Domain>>* domains) override;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_SCHEMA_HANDLER_H_
