// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_cache.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace cc {
namespace {

template <typename T>
void EraseFromMap(T* map, size_t n, const volatile PaintCacheId* ids) {
  for (size_t i = 0; i < n; ++i) {
    auto id = ids[i];
    map->erase(id);
  }
}

}  // namespace

ClientPaintCache::ClientPaintCache(size_t max_budget_bytes)
    : cache_map_(CacheMap::NO_AUTO_EVICT), max_budget_(max_budget_bytes) {}
ClientPaintCache::~ClientPaintCache() = default;

bool ClientPaintCache::Get(PaintDataType type, PaintCacheId id) {
  return cache_map_.Get(std::make_pair(type, id)) != cache_map_.end();
}

void ClientPaintCache::Put(PaintDataType type, PaintCacheId id, size_t size) {
  auto key = std::make_pair(type, id);
  DCHECK(cache_map_.Peek(key) == cache_map_.end());

  cache_map_.Put(key, size);
  bytes_used_ += size;
}

void ClientPaintCache::Purge(PurgedData* purged_data) {
  while (bytes_used_ > max_budget_) {
    auto it = cache_map_.rbegin();
    PaintDataType type = it->first.first;
    PaintCacheId id = it->first.second;

    (*purged_data)[static_cast<uint32_t>(type)].push_back(id);
    DCHECK_GE(bytes_used_, it->second);
    bytes_used_ -= it->second;
  }
}

bool ClientPaintCache::PurgeAll() {
  bool has_data = !cache_map_.empty();
  cache_map_.Clear();
  return has_data;
}

ServicePaintCache::ServicePaintCache() = default;
ServicePaintCache::~ServicePaintCache() = default;

void ServicePaintCache::PutTextBlob(PaintCacheId id, sk_sp<SkTextBlob> blob) {
  cached_blobs_.emplace(id, std::move(blob));
}

sk_sp<SkTextBlob> ServicePaintCache::GetTextBlob(PaintCacheId id) {
  auto it = cached_blobs_.find(id);
  return it == cached_blobs_.end() ? nullptr : it->second;
}

void ServicePaintCache::PutPath(PaintCacheId id, SkPath path) {
  cached_paths_.emplace(id, std::move(path));
}

SkPath* ServicePaintCache::GetPath(PaintCacheId id) {
  auto it = cached_paths_.find(id);
  return it == cached_paths_.end() ? nullptr : &it->second;
}

void ServicePaintCache::Purge(PaintDataType type,
                              size_t n,
                              const volatile PaintCacheId* ids) {
  switch (type) {
    case PaintDataType::kTextBlob:
      EraseFromMap(&cached_blobs_, n, ids);
      return;
    case PaintDataType::kPath:
      EraseFromMap(&cached_paths_, n, ids);
      return;
  }

  NOTREACHED();
}

void ServicePaintCache::PurgeAll() {
  cached_blobs_.clear();
  cached_paths_.clear();
}

}  // namespace cc
