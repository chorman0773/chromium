// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_

#include "components/ui_devtools/Overlay.h"
#include "components/ui_devtools/overlay_agent.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class Env;
}

namespace gfx {
class RenderText;
}

namespace ui_devtools {

enum HighlightRectsConfiguration {
  NO_DRAW,
  R1_CONTAINS_R2,
  R1_HORIZONTAL_FULL_LEFT_R2,
  R1_TOP_FULL_LEFT_R2,
  R1_BOTTOM_FULL_LEFT_R2,
  R1_TOP_PARTIAL_LEFT_R2,
  R1_BOTTOM_PARTIAL_LEFT_R2,
  R1_INTERSECTS_R2
};

enum RectSide { TOP_SIDE, LEFT_SIDE, RIGHT_SIDE, BOTTOM_SIDE };

class OverlayAgentAura : public OverlayAgent,
                         public ui::EventHandler,
                         public ui::LayerDelegate {
 public:
  OverlayAgentAura(DOMAgentAura* dom_agent, aura::Env* env);
  ~OverlayAgentAura() override;
  int pinned_id() const { return pinned_id_; };
  void SetPinnedNodeId(int pinned_id);

  // Overlay::Backend:
  protocol::Response setInspectMode(
      const String& in_mode,
      protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig)
      override;
  protocol::Response highlightNode(
      std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
      protocol::Maybe<int> node_id) override;
  protocol::Response hideHighlight() override;

  HighlightRectsConfiguration highlight_rect_config() const {
    return highlight_rect_config_;
  };

  // Return the id of the UI element located at |event|'s root location.
  // The function first searches for the targeted window, then the targeted
  // widget (if one exists), then the targeted view (if one exists). Return 0 if
  // no valid target is found.
  int FindElementIdTargetedByPoint(ui::LocatedEvent* event) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest,
                           MouseEventsGenerateFEEventsInInspectMode);
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightNonexistentNode);
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightWidget);
#if defined(USE_AURA)
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightWindow);
  FRIEND_TEST_ALL_PREFIXES(OverlayAgentTest, HighlightEmptyOrInvisibleWindow);
#endif
  protocol::Response HighlightNode(int node_id, bool show_size = false);
  // Returns true when there is any visible element to highlight.
  bool UpdateHighlight(
      const std::pair<gfx::NativeWindow, gfx::Rect>& window_and_bounds);

  // Shows the distances between the nodes identified by |pinned_id| and
  // |element_id| in the highlight overlay.
  void ShowDistancesInHighlightOverlay(int pinned_id, int element_id);

  // ui:EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer* layer_for_highlighting() { return layer_for_highlighting_.get(); }

  aura::Env* env_;
  std::unique_ptr<gfx::RenderText> render_text_;
  bool show_size_on_canvas_ = false;
  HighlightRectsConfiguration highlight_rect_config_;
  bool is_swap_ = false;

  std::unique_ptr<ui::Layer> layer_for_highlighting_;
  gfx::Rect hovered_rect_;
  gfx::Rect pinned_rect_;
  int pinned_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OverlayAgentAura);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_AURA_H_
