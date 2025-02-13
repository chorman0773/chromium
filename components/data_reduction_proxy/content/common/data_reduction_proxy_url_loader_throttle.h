// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_

#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "content/public/common/url_loader_throttle.h"

namespace data_reduction_proxy {

// Handles Data Reduction Proxy logic that needs to be applied to each request.
//
// This includes:
//   * Setting request headers for the data reduction proxy.
//   * Processing response headers from a data reduction proxy.
//   * Restarting the URL loader in order to use a different
//     proxy.
//   * Marking data reduction proxies to be bypassed for future
//     requests.
class DataReductionProxyURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  DataReductionProxyURLLoaderThrottle(
      const net::HttpRequestHeaders& post_cache_headers,
      mojom::DataReductionProxy* data_reduction_proxy);
  ~DataReductionProxyURLLoaderThrottle() override;

  // content::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers) override;
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::ResourceResponseHead& response_head,
      bool* defer) override;

 private:
  // Marks |bad_proxies| to be bypassed for |bypass_duration|. Once that action
  // has completed will call OnMarkProxiesAsBadComplete().
  void MarkProxiesAsBad(const std::vector<net::ProxyServer>& bad_proxies,
                        base::TimeDelta bypass_duration,
                        bool bypass_all);
  void OnMarkProxiesAsBadComplete();

  // Tells |delegate_| to restart the URL loader if |pending_restart_| was set.
  void DoPendingRestart();

  net::HttpRequestHeaders post_cache_headers_;

  // List of URLs in the redirect chain. |.front()| is the original URL
  // requested, and |.back()| is the latest URL that was redirected to.
  std::vector<GURL> url_chain_;
  std::string request_method_;

  // Shared controller for the Data Reduction Proxy. When the throttle is
  // running in the renderer process, this is a channel for sending commands to
  // browser process.
  mojom::DataReductionProxy* data_reduction_proxy_ = nullptr;

  // |pending_restart_| is set to true if the URL loader needs to be restarted
  // using |pending_restart_load_flags_|.
  int pending_restart_load_flags_ = 0;
  bool pending_restart_ = false;

  // Set to true while waiting for OnMarkProxiesAsBadComplete to run.
  bool waiting_for_mark_proxies_ = false;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
