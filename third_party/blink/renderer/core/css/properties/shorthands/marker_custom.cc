// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/marker.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {
namespace CSSShorthand {

bool Marker::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const CSSValue* marker = css_property_parser_helpers::ParseLonghand(
      CSSPropertyMarkerStart, CSSPropertyMarker, context, range);
  if (!marker || !range.AtEnd())
    return false;

  css_property_parser_helpers::AddProperty(
      CSSPropertyMarkerStart, CSSPropertyMarker, *marker, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyMarkerMid, CSSPropertyMarker, *marker, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyMarkerEnd, CSSPropertyMarker, *marker, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
