/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_br_element.h"

#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_br.h"

namespace blink {

using namespace html_names;

inline HTMLBRElement::HTMLBRElement(Document& document)
    : HTMLElement(kBrTag, document) {}

DEFINE_NODE_FACTORY(HTMLBRElement)

bool HTMLBRElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == kClearAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLBRElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == kClearAttr) {
    // If the string is empty, then don't add the clear property.
    // <br clear> and <br clear=""> are just treated like <br> by Gecko, Mac IE,
    // etc. -dwh
    if (!value.IsEmpty()) {
      if (DeprecatedEqualIgnoringCase(value, "all"))
        AddPropertyToPresentationAttributeStyle(style, CSSPropertyClear,
                                                CSSValueBoth);
      else
        AddPropertyToPresentationAttributeStyle(style, CSSPropertyClear, value);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

LayoutObject* HTMLBRElement::CreateLayoutObject(const ComputedStyle& style) {
  if (style.HasContent())
    return LayoutObject::CreateObject(this, style);
  return new LayoutBR(this);
}

}  // namespace blink
