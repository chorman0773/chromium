// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class PaintLayerPainterTest : public PaintControllerPaintTest {
  USING_FAST_MALLOC(PaintLayerPainterTest);

 public:
  void ExpectPaintedOutputInvisible(const char* element_name,
                                    bool expected_value) {
    // The optimization to skip painting for effectively-invisible content is
    // limited to pre-SPv2.
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      return;

    PaintLayer* target_layer =
        ToLayoutBox(GetLayoutObjectByElementId(element_name))->Layer();
    PaintLayerPaintingInfo painting_info(nullptr, LayoutRect(),
                                         kGlobalPaintNormalPhase, LayoutSize());
    bool invisible =
        PaintLayerPainter(*target_layer)
            .PaintedOutputInvisible(target_layer->GetLayoutObject().StyleRef(),
                                    painting_info.GetGlobalPaintFlags());
    EXPECT_EQ(expected_value, invisible)
        << "Failed painted output visibility, expected=" << expected_value
        << ", actual=" << invisible << "].";
  }

  PaintController& MainGraphicsLayerPaintController() {
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking(&GetLayoutView())
        ->GetPaintController();
  }

 private:
  void SetUp() override {
    PaintControllerPaintTest::SetUp();
    EnableCompositing();
  }
};

INSTANTIATE_PAINT_TEST_CASE_P(PaintLayerPainterTest);

TEST_P(PaintLayerPainterTest, CachedSubsequence) {
  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='filler1' style='position: relative; z-index: 2;
        width: 20px; height: 20px; background-color: gray'></div>
    <div id='container2' style='position: relative; z-index: 3;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='position: absolute; width: 100px;
          height: 100px; background-color: green;'></div>
    </div>
    <div id='filler2' style='position: relative; z-index: 4;
        width: 20px; height: 20px; background-color: gray'></div>
  )HTML");

  auto& container1 = *GetLayoutObjectByElementId("container1");
  auto& content1 = *GetLayoutObjectByElementId("content1");
  auto& filler1 = *GetLayoutObjectByElementId("filler1");
  auto& container2 = *GetLayoutObjectByElementId("container2");
  auto& content2 = *GetLayoutObjectByElementId("content2");
  auto& filler2 = *GetLayoutObjectByElementId("filler2");

  const auto& view_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&filler1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType),
                          IsSameId(&filler2, kBackgroundType)));

  auto* container1_layer = ToLayoutBoxModelObject(container1).Layer();
  auto* filler1_layer = ToLayoutBoxModelObject(filler1).Layer();
  auto* container2_layer = ToLayoutBoxModelObject(container2).Layer();
  auto* filler2_layer = ToLayoutBoxModelObject(filler2).Layer();
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  auto view_chunk_type = kDocumentBackgroundType;
  auto chunk_background_type = DisplayItem::kLayerChunkBackground;
  auto chunk_foreground_type =
      DisplayItem::kLayerChunkNormalFlowAndPositiveZOrderChildren;
  auto filler_chunk_type = DisplayItem::PaintPhaseToDrawingType(
      PaintPhase::kSelfBlockBackgroundOnly);

  auto check_chunks = [&]() {
    // Check that new paint chunks were forced for |container1| and
    // |container2|.
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 1, PaintChunk::Id(view_client, view_chunk_type),
                         chunk_state),
            IsPaintChunk(
                1, 2, PaintChunk::Id(*container1_layer, chunk_background_type),
                chunk_state),
            IsPaintChunk(
                2, 3, PaintChunk::Id(*container1_layer, chunk_foreground_type),
                chunk_state),
            IsPaintChunk(3, 4,
                         PaintChunk::Id(*filler1_layer, filler_chunk_type),
                         chunk_state),
            IsPaintChunk(
                4, 5, PaintChunk::Id(*container2_layer, chunk_background_type),
                chunk_state),
            IsPaintChunk(
                5, 6, PaintChunk::Id(*container2_layer, chunk_foreground_type),
                chunk_state),
            IsPaintChunk(6, 7,
                         PaintChunk::Id(*filler2_layer, filler_chunk_type),
                         chunk_state)));
  };

  check_chunks();

  ToHTMLElement(content1.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(6, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&filler1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType),
                          IsSameId(&filler2, kBackgroundType)));

  // We should still have the paint chunks forced by the cached subsequences.
  check_chunks();
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange) {
  // TODO(wangxianzhu): SPv2 deals with interest rect differently, so disable
  // this test for SPv2 temporarily.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
       width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2a' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
      <div id='content2b' style='position: absolute; top: 200px;
          width: 100px; height: 100px; background-color: green'></div>
    </div>
    <div id='container3' style='position: absolute; z-index: 2;
        left: 300px; top: 0; width: 200px; height: 200px;
        background-color: blue'>
      <div id='content3' style='position: absolute; width: 200px;
          height: 200px; background-color: green'></div>
    </div>
  )HTML");
  InvalidateAll(RootPaintController());

  LayoutObject& container1 =
      *GetDocument().getElementById("container1")->GetLayoutObject();
  LayoutObject& content1 =
      *GetDocument().getElementById("content1")->GetLayoutObject();
  LayoutObject& container2 =
      *GetDocument().getElementById("container2")->GetLayoutObject();
  LayoutObject& content2a =
      *GetDocument().getElementById("content2a")->GetLayoutObject();
  LayoutObject& content2b =
      *GetDocument().getElementById("content2b")->GetLayoutObject();
  LayoutObject& container3 =
      *GetDocument().getElementById("container3")->GetLayoutObject();
  LayoutObject& content3 =
      *GetDocument().getElementById("content3")->GetLayoutObject();

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 400, 300);
  Paint(&interest_rect);

  const auto& background_display_item_client = ViewScrollingBackgroundClient();

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&container3, kBackgroundType),
                          IsSameId(&content3, kBackgroundType)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect new_interest_rect(0, 100, 300, 1000);
  EXPECT_TRUE(PaintWithoutCommit(&new_interest_rect));

  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs empty subsequence
  // pair.
  EXPECT_EQ(5, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&content2b, kBackgroundType)));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnInterestRectChangeUnderInvalidationChecking) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);

  SetBodyInnerHTML(R"HTML(
    <style>p { width: 200px; height: 50px; background: green }</style>
    <div id='target' style='position: relative; z-index: 1'>
      <p></p><p></p><p></p><p></p>
    </div>
  )HTML");
  InvalidateAll(RootPaintController());

  // |target| will be fully painted.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 400, 300);
  Paint(&interest_rect);

  // |target| will be partially painted. Should not trigger under-invalidation
  // checking DCHECKs.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect new_interest_rect(0, 100, 300, 1000);
  Paint(&new_interest_rect);
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnStyleChangeWithInterestRectClipping) {
  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // PaintResult of all subsequences will be MayBeClippedByPaintDirtyRect.
  IntRect interest_rect(0, 0, 50, 300);
  Paint(&interest_rect);

  LayoutObject& container1 =
      *GetDocument().getElementById("container1")->GetLayoutObject();
  LayoutObject& content1 =
      *GetDocument().getElementById("content1")->GetLayoutObject();
  LayoutObject& container2 =
      *GetDocument().getElementById("container2")->GetLayoutObject();
  LayoutObject& content2 =
      *GetDocument().getElementById("content2")->GetLayoutObject();

  const auto& background_display_item_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));

  ToHTMLElement(content1.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit(&interest_rect));
  EXPECT_EQ(4, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceRetainsPreviousPaintResult) {
  // TODO(wangxianzhu): SPv2 deals with interest rect differently, so disable
  // this test for SPv2 temporarily.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      html, body { height: 100%; margin: 0 }
      ::-webkit-scrollbar { display:none }
    </style>
    <div id="target" style="height: 8000px; contain: paint">
      <div id="content1" style="height: 100px; background: blue"></div>
      <div style="height: 6000px"></div>
      <div id="content2" style="height: 100px; background: blue"></div>
    </div>
    <div id="change" style="display: none"></div>
  )HTML");

  const auto* target_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("target"))->Layer();
  const auto* content1 = GetLayoutObjectByElementId("content1");
  const auto* content2 = GetLayoutObjectByElementId("content2");
  const auto& view_client = ViewScrollingBackgroundClient();
  // |content2| is out of the interest rect.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                          IsSameId(content1, kBackgroundType)));
  // |target| is partially painted.
  EXPECT_EQ(kMayBeClippedByPaintDirtyRect, target_layer->PreviousPaintResult());
  EXPECT_EQ(LayoutRect(0, 0, 800, 4600),
            target_layer->PreviousPaintDirtyRect());
  // |target| created subsequence.
  EXPECT_SUBSEQUENCE(*target_layer, 1u, 2u);

  // Change something that triggers a repaint but |target| should use cached
  // subsequence.
  GetDocument().getElementById("change")->setAttribute(html_names::kStyleAttr,
                                                       "display: block");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(target_layer->NeedsRepaint());
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  // Painted result is the same.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                          IsSameId(content1, kBackgroundType)));
  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByPaintDirtyRect, target_layer->PreviousPaintResult());
  EXPECT_EQ(LayoutRect(0, 0, 800, 4600),
            target_layer->PreviousPaintDirtyRect());
  // |target| still created subsequence (cached).
  EXPECT_SUBSEQUENCE(*target_layer, 1u, 2u);

  // Scroll the view so that both |content1| and |content2| are in the interest
  // rect.
  GetLayoutView().GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 3000),
                                                       kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Scrolling doesn't set NeedsRepaint flag. Change of paint dirty rect of
  // a partially painted layer will trigger repaint.
  EXPECT_FALSE(target_layer->NeedsRepaint());
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  // Painted result should include both |content1| and |content2|.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                          IsSameId(content1, kBackgroundType),
                          IsSameId(content2, kBackgroundType)));
  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByPaintDirtyRect, target_layer->PreviousPaintResult());
  EXPECT_EQ(LayoutRect(0, 0, 800, 7600),
            target_layer->PreviousPaintDirtyRect());
  // |target| still created subsequence (repainted).
  EXPECT_SUBSEQUENCE(*target_layer, 1u, 3u);
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString style_without_outline =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_outline =
      "outline: 1px solid blue; " + style_without_outline;
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='outline'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& outline_div =
      *GetDocument().getElementById("outline")->GetLayoutObject();
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == outline_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());

  // Outline on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantOutlines.
  ToHTMLElement(self_painting_layer_object.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; outline: 1px solid green");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), self_painting_layer_object,
      DisplayItem::PaintPhaseToDrawingType(PaintPhase::kSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be set when any descendant on the
  // same layer has outline.
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_outline);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), outline_div,
      DisplayItem::PaintPhaseToDrawingType(PaintPhase::kSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString style_without_float =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_float = "float: left; " + style_without_float;
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == float_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());

  // needsPaintPhaseFloat should be set when any descendant on the same layer
  // has float.
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_float);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseFloat should be reset when there is no float actually
  // painted.
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <span id='span' style='position: relative'>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue; float: left'></div>
        </span>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  LayoutBoxModelObject& span = *ToLayoutBoxModelObject(
      GetDocument().getElementById("span")->GetLayoutObject());
  PaintLayer& span_layer = *span.Layer();
  ASSERT_TRUE(&span_layer == float_div.EnclosingLayer());
  ASSERT_FALSE(span_layer.NeedsPaintPhaseFloat());
  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());

  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(span_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhaseBlockBackground) {
  AtomicString style_without_background = "width: 50px; height: 50px";
  AtomicString style_with_background =
      "background: blue; " + style_without_background;
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='background'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& background_div =
      *GetDocument().getElementById("background")->GetLayoutObject();
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_background);
  UpdateAllLifecyclePhasesForTest();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == background_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  // Background on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantBlockBackgrounds.
  ToHTMLElement(self_painting_layer_object.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; background: green");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), self_painting_layer_object,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be set when any descendant
  // on the same layer has Background.
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_background);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), background_div,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be reset when no outline
  // is actually painted.
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_background);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-layer'>
      <div style='height: 100px'>
        <div style='height: 20px; outline: 1px solid red;
            background-color: green'>outline and background</div>
        <div style='float: left'>float</div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-layer")->GetLayoutObject());
  EXPECT_FALSE(layer_div.HasLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, "position: relative");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-self-painting' style='width: 100px; height: 100px;
    overflow: hidden'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-self-painting")->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  EXPECT_FALSE(layer_div.Layer()->IsSelfPaintingLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          "width: 100px; height: 100px; overflow: hidden; position: relative");
  UpdateAllLifecyclePhasesForTest();
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-non-self-painting' style='width: 100px; height: 100px;
    overflow: hidden; position: relative'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("will-be-non-self-painting")
                                  ->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "width: 100px; height: 100px; overflow: hidden");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgrounds) {
  // "position: relative" makes the table and td self-painting layers.
  // The table's layer should be marked needsPaintPhaseDescendantBlockBackground
  // because it will paint collapsed borders in the phase.
  SetBodyInnerHTML(R"HTML(
    <table id='table' style='position: relative; border-collapse: collapse'>
      <tr><td style='position: relative; border: 1px solid green'>
        Cell
      </td></tr>
    </table>
  )HTML");

  LayoutBoxModelObject& table =
      *ToLayoutBoxModelObject(GetLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.HasLayer());
  PaintLayer& layer = *table.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgroundsDynamic) {
  SetBodyInnerHTML(R"HTML(
    <table id='table' style='position: relative'>
      <tr><td style='position: relative; border: 1px solid green'>
        Cell
      </td></tr>
    </table>
  )HTML");

  LayoutBoxModelObject& table =
      *ToLayoutBoxModelObject(GetLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.HasLayer());
  PaintLayer& layer = *table.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_FALSE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(table.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: relative; border-collapse: collapse");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, DontPaintWithTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001'></div>");
  ExpectPaintedOutputInvisible("target", true);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001; "
      "    will-change: opacity'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndBackdropFilter) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    backdrop-filter: blur(2px);'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTinyOpacityAndBackdropFilterAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    backdrop-filter: blur(2px); will-change: opacity'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithCompositedTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    will-change: transform'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithNonTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.1'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithEffectAnimationZeroOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      animation-name: example;
      animation-duration: 4s;
    }
    @keyframes example {
      from { opacity: 0.0;}
      to { opacity: 1.0;}
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTransformAnimationZeroOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div#target {
      animation-name: example;
      animation-duration: 4s;
      opacity: 0.0;
    }
    @keyframes example {
      from { transform: translate(0px, 0px); }
      to { transform: translate(3em, 0px); }
    }
    </style>
    <div id='target'>x</div></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTransformAnimationZeroOpacityWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div#target {
      animation-name: example;
      animation-duration: 4s;
      opacity: 0.0;
      will-change: opacity;
    }
    @keyframes example {
      from { transform: translate(0px, 0px); }
      to { transform: translate(3em, 0px); }
    }
    </style>
    <div id='target'>x</div></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      opacity: 0;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithNoContentAndZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      opacity: 0;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisible("target", false);
}

}  // namespace blink
