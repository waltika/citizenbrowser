// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/citizen_x/network_service_citizennotes_observer.h"

#include "content/browser/citizen_x/citizennotes_agent_host_impl.h"
#include "content/browser/citizen_x/citizennotes_instrumentation.h"
#include "content/browser/citizen_x/protocol/cnaudits_handler.h"
#include "content/browser/citizen_x/protocol/cnnetwork_handler.h"
#include "content/browser/citizen_x/render_frame_citizennotes_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(CitizenNotesAgentHostImpl* agent_host,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  for (auto* h : Handler::ForAgentHost(agent_host))
    (h->*method)(std::forward<Args>(args)...);
}

RenderFrameHostImpl* GetRenderFrameHostImplFrom(int frame_tree_node_id) {
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn) {
    return nullptr;
  }

  RenderFrameHostImpl* rfhi = ftn->current_frame_host();
  return rfhi;
}

}  // namespace

NetworkServiceCitizenNotesObserver::NetworkServiceCitizenNotesObserver(
    base::PassKey<NetworkServiceCitizenNotesObserver> pass_key,
    const std::string& id,
    int frame_tree_node_id)
    : citizennotes_agent_id_(id), frame_tree_node_id_(frame_tree_node_id) {}

NetworkServiceCitizenNotesObserver::~NetworkServiceCitizenNotesObserver() = default;

CitizenNotesAgentHostImpl* NetworkServiceCitizenNotesObserver::GetCitizenNotesAgentHost() {
  if (frame_tree_node_id_ != FrameTreeNode::kFrameTreeNodeInvalidId) {
    auto* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (!frame_tree_node)
      return nullptr;
    return RenderFrameCitizenNotesAgentHost::GetFor(frame_tree_node);
  }
  auto host = CitizenNotesAgentHostImpl::GetForId(citizennotes_agent_id_);
  if (!host)
    return nullptr;
  return host.get();
}

void NetworkServiceCitizenNotesObserver::OnRawRequest(
    const std::string& citizennotes_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    std::vector<network::mojom::HttpRawHeaderPairPtr> request_headers,
    base::TimeTicks timestamp,
    network::mojom::ClientSecurityStatePtr security_state,
    network::mojom::CNOtherPartitionInfoPtr other_partition_info) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(host,
                   &protocol::CNNetworkHandler::OnRequestWillBeSentExtraInfo,
                   citizennotes_request_id, request_cookie_list, request_headers,
                   timestamp, security_state, other_partition_info);
}

void NetworkServiceCitizenNotesObserver::OnRawResponse(
    const std::string& citizennotes_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    std::vector<network::mojom::HttpRawHeaderPairPtr> response_headers,
    const absl::optional<std::string>& response_headers_text,
    network::mojom::IPAddressSpace resource_address_space,
    int32_t http_status_code,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(host, &protocol::CNNetworkHandler::OnResponseReceivedExtraInfo,
                   citizennotes_request_id, response_cookie_list, response_headers,
                   response_headers_text, resource_address_space,
                   http_status_code, cookie_partition_key);
}

void NetworkServiceCitizenNotesObserver::OnTrustTokenOperationDone(
    const std::string& citizennotes_request_id,
    network::mojom::TrustTokenOperationResultPtr result) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(host, &protocol::CNNetworkHandler::OnTrustTokenOperationDone,
                   citizennotes_request_id, *result);
}

void NetworkServiceCitizenNotesObserver::OnPrivateNetworkRequest(
    const absl::optional<std::string>& citizennotes_request_id,
    const GURL& url,
    bool is_warning,
    network::mojom::IPAddressSpace resource_address_space,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  if (frame_tree_node_id_ == FrameTreeNode::kFrameTreeNodeInvalidId)
    return;
  auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!ftn)
    return;
  auto cors_error_status =
      protocol::Network::CorsErrorStatus::Create()
          .SetCorsError(
              protocol::Network::CorsErrorEnum::InsecurePrivateNetwork)
          .SetFailedParameter("")
          .Build();
  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(citizennotes_request_id.value_or(""))
          .SetUrl(url.spec())
          .Build();
  auto cors_issue_details =
      protocol::Audits::CorsIssueDetails::Create()
          .SetIsWarning(is_warning)
          .SetResourceIPAddressSpace(
              protocol::CNNetworkHandler::BuildIpAddressSpace(
                  resource_address_space))
          .SetRequest(std::move(affected_request))
          .SetCorsErrorStatus(std::move(cors_error_status))
          .Build();
  if (auto maybe_protocol_security_state =
          protocol::CNNetworkHandler::MaybeBuildClientSecurityState(
              client_security_state)) {
    cors_issue_details->SetClientSecurityState(
        std::move(maybe_protocol_security_state));
  }
  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCorsIssueDetails(std::move(cors_issue_details))
                     .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::CorsIssue)
                   .SetDetails(std::move(details))
                   .Build();
  citizennotes_instrumentation::ReportBrowserInitiatedIssue(
      ftn->current_frame_host(), issue.get());
}

void NetworkServiceCitizenNotesObserver::OnCorsPreflightRequest(
    const base::UnguessableToken& citizennotes_request_id,
    const net::HttpRequestHeaders& request_headers,
    network::mojom::URLRequestCitizenNotesInfoPtr request_info,
    const GURL& initiator_url,
    const std::string& initiator_citizennotes_request_id) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  auto timestamp = base::TimeTicks::Now();
  auto id = citizennotes_request_id.ToString();
  DispatchToAgents(host, &protocol::CNNetworkHandler::RequestSent, id,
                   /* loader_id=*/"", request_headers, *request_info,
                   protocol::Network::Initiator::TypeEnum::Preflight,
                   initiator_url, initiator_citizennotes_request_id, timestamp);
}

void NetworkServiceCitizenNotesObserver::OnCorsPreflightResponse(
    const base::UnguessableToken& citizennotes_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadCitizenNotesInfoPtr head) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  auto id = citizennotes_request_id.ToString();
  DispatchToAgents(host, &protocol::CNNetworkHandler::ResponseReceived, id,
                   /* loader_id=*/"", url,
                   protocol::Network::ResourceTypeEnum::Preflight, *head,
                   protocol::Maybe<std::string>());
}

void NetworkServiceCitizenNotesObserver::OnCorsPreflightRequestCompleted(
    const base::UnguessableToken& citizennotes_request_id,
    const network::URLLoaderCompletionStatus& status) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  auto id = citizennotes_request_id.ToString();
  DispatchToAgents(host, &protocol::CNNetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Preflight, status);
}

void NetworkServiceCitizenNotesObserver::OnCorsError(
    const absl::optional<std::string>& citizennotes_request_id,
    const absl::optional<::url::Origin>& initiator_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const GURL& url,
    const network::CorsErrorStatus& cors_error_status,
    bool is_warning) {
  RenderFrameHostImpl* rfhi = GetRenderFrameHostImplFrom(frame_tree_node_id_);
  if (!rfhi)
    return;

  // TODO(https://crbug.com/1268378): Remove this once enforcement is always
  // enabled and warnings are no more.
  if (is_warning && initiator_origin.has_value()) {
    if (!initiator_origin->IsSameOriginWith(url)) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          rfhi, blink::mojom::WebFeature::
                    kPrivateNetworkAccessIgnoredCrossOriginPreflightError);
    }

    if (net::SchemefulSite(initiator_origin.value()) !=
        net::SchemefulSite(url)) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          rfhi, blink::mojom::WebFeature::
                    kPrivateNetworkAccessIgnoredCrossSitePreflightError);
    }
  }

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(citizennotes_request_id ? *citizennotes_request_id : "")
          .SetUrl(url.spec())
          .Build();

  auto cors_issue_details =
      protocol::Audits::CorsIssueDetails::Create()
          .SetIsWarning(is_warning)
          .SetRequest(std::move(affected_request))
          .SetCorsErrorStatus(
              protocol::CNNetworkHandler::BuildCorsErrorStatus(cors_error_status))
          .Build();
  if (initiator_origin) {
    cors_issue_details->SetInitiatorOrigin(initiator_origin->GetURL().spec());
  }
  if (auto maybe_protocol_security_state =
          protocol::CNNetworkHandler::MaybeBuildClientSecurityState(
              client_security_state)) {
    cors_issue_details->SetClientSecurityState(
        std::move(maybe_protocol_security_state));
  }

  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCorsIssueDetails(std::move(cors_issue_details))
                     .Build();
  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::CorsIssue)
                   .SetDetails(std::move(details))
                   .SetIssueId(cors_error_status.issue_id.ToString())
                   .Build();
  citizennotes_instrumentation::ReportBrowserInitiatedIssue(rfhi, issue.get());
}

void NetworkServiceCitizenNotesObserver::OnCorbError(
    const absl::optional<std::string>& citizennotes_request_id,
    const GURL& url) {
  RenderFrameHostImpl* rfhi = GetRenderFrameHostImplFrom(frame_tree_node_id_);
  if (!rfhi) {
    return;
  }

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request =
      protocol::Audits::AffectedRequest::Create()
          .SetRequestId(citizennotes_request_id.value_or(""))
          .SetUrl(url.spec())
          .Build();
  auto generic_details =
      protocol::Audits::GenericIssueDetails::Create()
          .SetErrorType(protocol::Audits::GenericIssueErrorTypeEnum::
                            ResponseWasBlockedByORB)
          .SetRequest(std::move(affected_request))
          .Build();
  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetGenericIssueDetails(std::move(generic_details))
                     .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::GenericIssue)
          .SetDetails(std::move(details))
          .Build();
  citizennotes_instrumentation::ReportBrowserInitiatedIssue(rfhi, issue.get());
}

void NetworkServiceCitizenNotesObserver::OnSubresourceWebBundleMetadata(
    const std::string& citizennotes_request_id,
    const std::vector<GURL>& urls) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(host,
                   &protocol::CNNetworkHandler::OnSubresourceWebBundleMetadata,
                   citizennotes_request_id, urls);
}

void NetworkServiceCitizenNotesObserver::OnSubresourceWebBundleMetadataError(
    const std::string& citizennotes_request_id,
    const std::string& error_message) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::CNNetworkHandler::OnSubresourceWebBundleMetadataError,
      citizennotes_request_id, error_message);
}

void NetworkServiceCitizenNotesObserver::OnSubresourceWebBundleInnerResponse(
    const std::string& inner_request_citizennotes_id,
    const GURL& url,
    const absl::optional<std::string>& bundle_request_citizennotes_id) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::CNNetworkHandler::OnSubresourceWebBundleInnerResponse,
      inner_request_citizennotes_id, url, bundle_request_citizennotes_id);
}

void NetworkServiceCitizenNotesObserver::OnSubresourceWebBundleInnerResponseError(
    const std::string& inner_request_citizennotes_id,
    const GURL& url,
    const std::string& error_message,
    const absl::optional<std::string>& bundle_request_citizennotes_id) {
  auto* host = GetCitizenNotesAgentHost();
  if (!host)
    return;
  DispatchToAgents(
      host, &protocol::CNNetworkHandler::OnSubresourceWebBundleInnerResponseError,
      inner_request_citizennotes_id, url, error_message,
      bundle_request_citizennotes_id);
}

void NetworkServiceCitizenNotesObserver::Clone(
    mojo::PendingReceiver<network::mojom::CitizenNotesObserver> observer) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceCitizenNotesObserver>(
          base::PassKey<NetworkServiceCitizenNotesObserver>(), citizennotes_agent_id_,
          frame_tree_node_id_),
      std::move(observer));
}

mojo::PendingRemote<network::mojom::CitizenNotesObserver>
NetworkServiceCitizenNotesObserver::MakeSelfOwned(const std::string& id) {
  mojo::PendingRemote<network::mojom::CitizenNotesObserver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceCitizenNotesObserver>(
          base::PassKey<NetworkServiceCitizenNotesObserver>(), id,
          FrameTreeNode::kFrameTreeNodeInvalidId),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::CitizenNotesObserver>
NetworkServiceCitizenNotesObserver::MakeSelfOwned(FrameTreeNode* frame_tree_node) {
  mojo::PendingRemote<network::mojom::CitizenNotesObserver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetworkServiceCitizenNotesObserver>(
          base::PassKey<NetworkServiceCitizenNotesObserver>(), std::string(),
          frame_tree_node->frame_tree_node_id()),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

}  // namespace content
