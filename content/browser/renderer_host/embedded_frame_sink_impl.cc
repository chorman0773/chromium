// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/embedded_frame_sink_impl.h"

#include <memory>
#include <utility>

#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

EmbeddedFrameSinkImpl::EmbeddedFrameSinkImpl(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    blink::mojom::EmbeddedFrameSinkClientPtr client,
    DestroyCallback destroy_callback)
    : host_frame_sink_manager_(host_frame_sink_manager),
      client_(std::move(client)),
      parent_frame_sink_id_(parent_frame_sink_id),
      frame_sink_id_(frame_sink_id) {
  client_.set_connection_error_handler(std::move(destroy_callback));
  host_frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kYes);
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "EmbeddedFrameSinkImpl");
}

EmbeddedFrameSinkImpl::~EmbeddedFrameSinkImpl() {
  if (has_created_compositor_frame_sink_) {
    host_frame_sink_manager_->UnregisterFrameSinkHierarchy(
        parent_frame_sink_id_, frame_sink_id_);
  }
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void EmbeddedFrameSinkImpl::CreateCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClientPtr client,
    viz::mojom::CompositorFrameSinkRequest request) {
  // We might recreate the CompositorFrameSink on context loss or GPU crash.
  // Only register frame sink hierarchy the first time.
  if (!has_created_compositor_frame_sink_) {
    // The request to create an embedded frame sink and the lifetime of the
    // parent are controlled by different IPC channels. It's possible the parent
    // FrameSinkId has been invalidated by the time this request has arrived. In
    // that case, drop the request since there is no embedder.
    if (!host_frame_sink_manager_->RegisterFrameSinkHierarchy(
            parent_frame_sink_id_, frame_sink_id_)) {
      return;
    }
  }

  host_frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request), std::move(client));

  has_created_compositor_frame_sink_ = true;
}

void EmbeddedFrameSinkImpl::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), frame_sink_id_);

  local_surface_id_ = surface_info.id().local_surface_id();
  if (client_)
    client_->OnFirstSurfaceActivation(surface_info);
}

void EmbeddedFrameSinkImpl::OnFrameTokenChanged(uint32_t frame_token) {
  // TODO(yiyix, fsamuel): To complete plumbing of frame tokens for offscreen
  // canvas
}

}  // namespace content
