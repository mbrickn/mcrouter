/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <folly/Range.h>

#include "mcrouter/McrouterFiberContext.h"
#include "mcrouter/Proxy.h"
#include "mcrouter/ProxyConfig.h"
#include "mcrouter/ProxyRequestContext.h"
#include "mcrouter/lib/PoolContext.h"
#include "mcrouter/lib/RouteHandleTraverser.h"
#include "mcrouter/routes/McrouterRouteHandle.h"
#include "mcrouter/routes/ProxyRoute.h"

namespace facebook {
namespace memcache {
namespace mcrouter {

/**
 * Iterates over possible clients for the given key with the templated
 * request type. This includes shadows, shard splits, etc.
 * Failover is ignored by default (failovers will be listed if
 * includeFailoverDestinations is set to true).
 * The first client (if there is some) is for "fast" route, i.e. the one we
 * synchronously route to.
 */
template <class Request, class RouterInfo>
typename std::enable_if<
    ListContains<typename RouterInfo::RoutableRequests, Request>::value,
    void>::type
foreachPossibleClient(
    Proxy<RouterInfo>& proxy,
    folly::StringPiece key,
    std::function<void(const PoolContext&, const AccessPoint&)> clientCallback,
    std::function<void(const ShardSplitter&)> spCallback = nullptr,
    bool includeFailoverDestinations = false) {
  Request req(key);
  foreachPossibleClient(
      proxy,
      req,
      std::move(clientCallback),
      std::move(spCallback),
      includeFailoverDestinations);
}

template <class Request, class RouterInfo>
typename std::enable_if<
    ListContains<typename RouterInfo::RoutableRequests, Request>::value,
    void>::type
foreachPossibleClient(
    Proxy<RouterInfo>& proxy,
    const Request& req,
    std::function<void(const PoolContext&, const AccessPoint&)> clientCallback,
    std::function<void(const ShardSplitter&)> spCallback = nullptr,
    bool includeFailoverDestinations = false) {
  auto ctx = ProxyRequestContextWithInfo<RouterInfo>::createRecording(
      proxy, std::move(clientCallback), std::move(spCallback));
  {
    auto p = proxy.getConfigLocked();
    fiber_local<RouterInfo>::runWithLocals(
        [&p,
         &req,
         ctx = std::move(ctx),
         includeFailoverDestinations]() mutable {
          fiber_local<RouterInfo>::setSharedCtx(std::move(ctx));
          fiber_local<RouterInfo>::setFailoverDisabled(
              !includeFailoverDestinations);
          RouteHandleTraverser<typename RouterInfo::RouteHandleIf> t;
          p.second.proxyRoute().traverse(req, t);
        });
  }
}

} // mcrouter
} // memcache
} // facebook
