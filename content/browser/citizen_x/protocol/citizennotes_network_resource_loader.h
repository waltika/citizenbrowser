// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_NETWORK_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_NETWORK_RESOURCE_LOADER_H_

#include <memory>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
namespace protocol {

// The CitizenNotesNetworkResourceLoader loads a network resource for CitizenNotes
// and passes it to the provided call-back once loading completed. Currently,
// the resource is provided as a string, but in the future this will use
// a CitizenNotesStreamPipe. This is why we don't just use DownloadToString.

class CONTENT_EXPORT CitizenNotesNetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  using CompletionCallback =
      base::OnceCallback<void(CitizenNotesNetworkResourceLoader*,
                              const net::HttpResponseHeaders* rh,
                              bool success,
                              int net_error,
                              std::string content)>;
  enum class Caching { kBypass, kDefault };
  enum class Credentials { kInclude, kSameSite };

  // The |origin| and |site_for_cookies| parameters are supplied by the caller,
  // and we trust the caller that these values are reasonable. They are usually
  // taken from a renderer host / worker host that was identified by the
  // CitizenNotes front-end based on the inspected page.
  static std::unique_ptr<CitizenNotesNetworkResourceLoader> Create(
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory,
      GURL gurl,
      const url::Origin& origin,
      net::SiteForCookies site_for_cookies,
      Caching caching,
      Credentials include_credentials,
      CompletionCallback complete_callback);

  ~CitizenNotesNetworkResourceLoader() override;

  // Disallow copy and assignment.
  CitizenNotesNetworkResourceLoader(const CitizenNotesNetworkResourceLoader&) = delete;
  CitizenNotesNetworkResourceLoader& operator=(
      const CitizenNotesNetworkResourceLoader&) = delete;

 private:
  CitizenNotesNetworkResourceLoader(
      network::ResourceRequest resource_request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory,
      CompletionCallback complete_callback);
  void DownloadAsStream();

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override;

  void OnComplete(bool success) override;

  void OnRetry(base::OnceClosure start_retry) override;

  const network::ResourceRequest resource_request_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  CompletionCallback completion_callback_;
  std::string content_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_CITIZENNOTES_PROTOCOL_CITIZENNOTES_NETWORK_RESOURCE_LOADER_H_
