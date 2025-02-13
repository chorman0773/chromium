// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_metadata_observer_impl.h"

#include <cmath>

#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace content {

namespace {
#if defined(OS_ANDROID)
constexpr float kEdgeThreshold = 10.0f;
#endif
}

RenderFrameMetadataObserverImpl::RenderFrameMetadataObserverImpl(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)),
      client_info_(std::move(client_info)),
      render_frame_metadata_observer_binding_(this) {}

RenderFrameMetadataObserverImpl::~RenderFrameMetadataObserverImpl() {}

void RenderFrameMetadataObserverImpl::BindToCurrentThread() {
  DCHECK(request_.is_pending());
  render_frame_metadata_observer_binding_.Bind(std::move(request_));
  render_frame_metadata_observer_client_.Bind(std::move(client_info_));
}

void RenderFrameMetadataObserverImpl::OnRenderFrameSubmission(
    const cc::RenderFrameMetadata& render_frame_metadata,
    viz::CompositorFrameMetadata* compositor_frame_metadata) {
  // By default only report metadata changes for fields which have a low
  // frequency of change. However if there are changes in high frequency
  // fields these can be reported while testing is enabled.
  bool send_metadata = false;
  bool needs_activation_notification = true;
  if (render_frame_metadata_observer_client_) {
    if (report_all_frame_submissions_for_testing_enabled_) {
      last_frame_token_ = compositor_frame_metadata->frame_token;
      compositor_frame_metadata->send_frame_token_to_embedder = true;
      render_frame_metadata_observer_client_->OnFrameSubmissionForTesting(
          last_frame_token_);
      send_metadata = !last_render_frame_metadata_ ||
                      *last_render_frame_metadata_ != render_frame_metadata;
    } else {
      send_metadata = !last_render_frame_metadata_ ||
                      ShouldSendRenderFrameMetadata(
                          *last_render_frame_metadata_, render_frame_metadata,
                          &needs_activation_notification);
    }
  }

  // Allways cache the full metadata, so that it can correctly be sent upon
  // ReportAllFrameSubmissionsForTesting. This must only be done after we've
  // compared the two for changes.
  last_render_frame_metadata_ = render_frame_metadata;

  // If the metadata is different, updates all the observers; or the metadata is
  // generated for first time and same as the default value, update the default
  // value to all the observers.
  if (send_metadata && render_frame_metadata_observer_client_) {
    // Sending |root_scroll_offset| outside of tests would leave the browser
    // process with out of date information. It is an optional parameter
    // which we clear here.
    auto metadata_copy = render_frame_metadata;
#if !defined(OS_ANDROID)
    if (!report_all_frame_submissions_for_testing_enabled_)
      metadata_copy.root_scroll_offset = base::nullopt;
#endif

    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder =
        needs_activation_notification;
    render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
        needs_activation_notification ? last_frame_token_ : 0u, metadata_copy);
  }

  // Always cache the initial frame token, so that if a test connects later on
  // it can be notified of the initial state.
  if (!last_frame_token_) {
    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder =
        needs_activation_notification;
  }
}

void RenderFrameMetadataObserverImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  report_all_frame_submissions_for_testing_enabled_ = enabled;

  if (!enabled || !last_frame_token_)
    return;

  // When enabled for testing send the cached metadata.
  DCHECK(render_frame_metadata_observer_client_);
  DCHECK(last_render_frame_metadata_.has_value());
  render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
      last_frame_token_, *last_render_frame_metadata_);
}

// static
bool RenderFrameMetadataObserverImpl::ShouldSendRenderFrameMetadata(
    const cc::RenderFrameMetadata& rfm1,
    const cc::RenderFrameMetadata& rfm2,
    bool* needs_activation_notification) {
  if (rfm1.root_background_color != rfm2.root_background_color ||
      rfm1.is_scroll_offset_at_top != rfm2.is_scroll_offset_at_top ||
      rfm1.selection != rfm2.selection ||
      rfm1.page_scale_factor != rfm2.page_scale_factor ||
      rfm1.external_page_scale_factor != rfm2.external_page_scale_factor ||
      rfm1.is_mobile_optimized != rfm2.is_mobile_optimized ||
      rfm1.device_scale_factor != rfm2.device_scale_factor ||
      rfm1.viewport_size_in_pixels != rfm2.viewport_size_in_pixels ||
      rfm1.top_controls_height != rfm2.top_controls_height ||
      rfm1.top_controls_shown_ratio != rfm2.top_controls_shown_ratio ||
      rfm1.local_surface_id_allocation != rfm2.local_surface_id_allocation) {
    *needs_activation_notification = true;
    return true;
  }

#if defined(OS_ANDROID)
  if (rfm1.bottom_controls_height != rfm2.bottom_controls_height ||
      rfm1.bottom_controls_shown_ratio != rfm2.bottom_controls_shown_ratio ||
      rfm1.min_page_scale_factor != rfm2.min_page_scale_factor ||
      rfm1.max_page_scale_factor != rfm2.max_page_scale_factor ||
      rfm1.root_overflow_y_hidden != rfm2.root_overflow_y_hidden ||
      rfm1.scrollable_viewport_size != rfm2.scrollable_viewport_size ||
      rfm1.root_layer_size != rfm2.root_layer_size ||
      rfm1.has_transparent_background != rfm2.has_transparent_background) {
    *needs_activation_notification = true;
    return true;
  }

  gfx::Vector2dF old_root_scroll_offset =
      rfm1.root_scroll_offset.value_or(gfx::Vector2dF());
  gfx::Vector2dF new_root_scroll_offset =
      rfm2.root_scroll_offset.value_or(gfx::Vector2dF());
  gfx::RectF old_viewport_rect(
      gfx::PointF(old_root_scroll_offset.x(), old_root_scroll_offset.y()),
      rfm1.scrollable_viewport_size);
  gfx::RectF new_viewport_rect(
      gfx::PointF(new_root_scroll_offset.x(), new_root_scroll_offset.y()),
      rfm2.scrollable_viewport_size);
  gfx::RectF new_root_layer_rect(rfm2.root_layer_size);
  bool at_left_or_right_edge =
      rfm2.root_layer_size.width() > rfm2.scrollable_viewport_size.width() &&
      (std::abs(new_viewport_rect.right() - new_root_layer_rect.right()) <
           kEdgeThreshold ||
       std::abs(new_viewport_rect.x() - new_root_layer_rect.x()) <
           kEdgeThreshold);

  bool at_top_or_bottom_edge =
      rfm2.root_layer_size.height() > rfm2.scrollable_viewport_size.height() &&
      (std::abs(new_viewport_rect.y() - new_root_layer_rect.y()) <
           kEdgeThreshold ||
       std::abs(new_viewport_rect.bottom() - new_root_layer_rect.bottom()) <
           kEdgeThreshold);

  if (old_viewport_rect != new_viewport_rect &&
      (at_left_or_right_edge || at_top_or_bottom_edge)) {
    *needs_activation_notification = false;
    return true;
  }
#endif

  *needs_activation_notification = false;
  return false;
}

}  // namespace content
