// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_location_bar_model.h"

#include "base/strings/utf_string_conversions.h"

#if defined(TOOLKIT_VIEWS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

TestLocationBarModel::TestLocationBarModel()
    : security_level_(security_state::NONE),
#if defined(TOOLKIT_VIEWS)
      icon_(&omnibox::kHttpIcon),
#endif
      should_display_url_(true) {
}

TestLocationBarModel::~TestLocationBarModel() {}

base::string16 TestLocationBarModel::GetFormattedFullURL() const {
  if (!formatted_full_url_)
    return base::UTF8ToUTF16(url_.spec());

  return *formatted_full_url_;
}

base::string16 TestLocationBarModel::GetURLForDisplay() const {
  if (!url_for_display_)
    return base::UTF8ToUTF16(url_.spec());

  return *url_for_display_;
}

GURL TestLocationBarModel::GetURL() const {
  return url_;
}

security_state::SecurityLevel TestLocationBarModel::GetSecurityLevel(
    bool ignore_editing) const {
  return security_level_;
}

bool TestLocationBarModel::IsSecurityInfoInitialized() const {
  return true;
}

const gfx::VectorIcon& TestLocationBarModel::GetVectorIcon() const {
  return *icon_;
}

base::string16 TestLocationBarModel::GetSecureDisplayText() const {
  return base::string16();
}

base::string16 TestLocationBarModel::GetSecureAccessibilityText() const {
  return base::string16();
}

base::string16 TestLocationBarModel::GetEVCertName() const {
  return (security_level_ == security_state::EV_SECURE) ? ev_cert_name_
                                                        : base::string16();
}

bool TestLocationBarModel::ShouldDisplayURL() const {
  return should_display_url_;
}

bool TestLocationBarModel::IsOfflinePage() const {
  return offline_page_;
}
