// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_NETWORK_SERVICE_CITIZENNOTES_OBSERVER_H_
#define CONTENT_BROWSER_CITIZENNOTES_NETWORK_SERVICE_CITIZENNOTES_OBSERVER_H_

#include <string>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/citizennotes_observer.mojom.h"

namespace content {

class CitizenNotesAgentHostImpl;
class FrameTreeNode;

// A springboard class to be able to bind to the network service as a
// CitizenNotesObserver but not requiring the creation of a CitizenNotesAgentHostImpl.
class NetworkServiceCitizenNotesObserver : public network::mojom::CitizenNotesObserver {
 public:
  NetworkServiceCitizenNotesObserver(
      base::PassKey<NetworkServiceCitizenNotesObserver> pass_key,
      const std::string& citizennotes_agent_id,
      int frame_tree_node_id);
  ~NetworkServiceCitizenNotesObserver() override;

  static mojo::PendingRemote<network::mojom::CitizenNotesObserver> MakeSelfOwned(
      const std::string& id);
  static mojo::PendingRemote<network::mojom::CitizenNotesObserver> MakeSelfOwned(
      FrameTreeNode* frame_tree_node);

 private:
  // network::mojom::CitizenNotesObserver overrides.
  void OnRawRequest(
      const std::string& citizennotes_request_id,
      const net::CookieAccessResultList& request_cookie_list,
      std::vector<network::mojom::HttpRawHeaderPairPtr> request_headers,
      const base::TimeTicks timestamp,
      network::mojom::ClientSecurityStatePtr security_state,
      network::mojom::CNOtherPartitionInfoPtr other_partition_info) override;
  void OnRawResponse(
      const std::string& citizennotes_request_id,
      const net::CookieAndLineAccessResultList& response_cookie_list,
      std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers,
      const absl::optional<std::string>& response_headers_text,
      network::mojom::IPAddressSpace resource_address_space,
      int32_t http_status_code,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key)
      override;
  void OnTrustTokenOperationDone(
      const std::string& citizennotes_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override;
  void OnPrivateNetworkRequest(
      const absl::optional<std::string>& citizennotes_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override;
  void OnCorsPreflightRequest(
      const base::UnguessableToken& citizennotes_request_id,
      const net::HttpRequestHeaders& request_headers,
      network::mojom::URLRequestCitizenNotesInfoPtr request_info,
      const GURL& initiator_url,
      const std::string& initiator_citizennotes_request_id) override;
  void OnCorsPreflightResponse(
      const base::UnguessableToken& citizennotes_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadCitizenNotesInfoPtr head) override;
  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& citizennotes_request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void OnCorsError(const absl::optional<std::string>& devtool_request_id,
                   const absl::optional<::url::Origin>& initiator_origin,
                   network::mojom::ClientSecurityStatePtr client_security_state,
                   const GURL& url,
                   const network::CorsErrorStatus& status,
                   bool is_warning) override;
  void OnCorbError(const absl::optional<std::string>& citizennotes_request_id,
                   const GURL& url) override;
  void OnSubresourceWebBundleMetadata(const std::string& citizennotes_request_id,
                                      const std::vector<GURL>& urls) override;
  void OnSubresourceWebBundleMetadataError(
      const std::string& citizennotes_request_id,
      const std::string& error_message) override;
  void OnSubresourceWebBundleInnerResponse(
      const std::string& inner_request_citizennotes_id,
      const GURL& url,
      const absl::optional<std::string>& bundle_request_citizennotes_id) override;
  void OnSubresourceWebBundleInnerResponseError(
      const std::string& inner_request_citizennotes_id,
      const GURL& url,
      const std::string& error_message,
      const absl::optional<std::string>& bundle_request_citizennotes_id) override;
  void Clone(mojo::PendingReceiver<network::mojom::CitizenNotesObserver> listener)
      override;

  CitizenNotesAgentHostImpl* GetCitizenNotesAgentHost();

  // This will be set for citizennotes observers that are created with a worker
  // context, empty otherwise.
  const std::string citizennotes_agent_id_;

  // This will be set for citizennotes observers that are created with a frame
  // context, otherwise it will be kFrameTreeNodeInvalidId.
  const int frame_tree_node_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_NETWORK_SERVICE_CITIZENNOTES_OBSERVER_H_
