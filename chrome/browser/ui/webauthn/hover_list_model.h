// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_

#include <stddef.h>

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/vector_icon_types.h"

// List model that controls which item is added to WebAuthN UI views.
// HoverListModel is observed by Observer which represents the actual
// UI view component.
class HoverListModel {
 public:
  enum class ListItemChangeType {
    kAddToViewComponent,
    kRemoveFromViewComponent,
  };

  class Observer {
   public:
    virtual void OnListItemAdded(int item_tag) = 0;
    virtual void OnListItemRemoved(int removed_list_item_tag) = 0;
    virtual void OnListItemChanged(int changed_list_item_tag,
                                   ListItemChangeType type) = 0;
  };

  HoverListModel() = default;
  virtual ~HoverListModel() = default;

  virtual bool ShouldShowPlaceholderForEmptyList() const = 0;
  virtual base::string16 GetPlaceholderText() const = 0;
  virtual const gfx::VectorIcon& GetPlaceholderIcon() const = 0;
  virtual std::vector<int> GetItemTags() const = 0;
  virtual base::string16 GetItemText(int item_tag) const = 0;
  virtual const gfx::VectorIcon& GetItemIcon(int item_tag) const = 0;
  virtual void OnListItemSelected(int item_tag) = 0;

  void SetObserver(Observer* observer) {
    DCHECK(!observer_);
    observer_ = observer;
  }

  void RemoveObserver() { observer_ = nullptr; }

 protected:
  Observer* observer() { return observer_; }

 private:
  Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HoverListModel);
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
