// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
#define CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/net/net_error_helper_core.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"

namespace content {
class RenderFrame;
}

// This class makes various helper functions available to the
// error page loaded by NetErrorHelper.  It is bound to the JavaScript
// window.errorPageController object.
class NetErrorPageController : public gin::Wrappable<NetErrorPageController> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Interface used to notify creator of user actions invoked on the error page.
  class Delegate {
   public:
    // Button press notification from error page.
    virtual void ButtonPressed(NetErrorHelperCore::Button button) = 0;

    // Called when a link with the given tracking ID is pressed.
    virtual void TrackClick(int tracking_id) = 0;

    // Called to open suggested offline content when it is pressed.
    virtual void LaunchOfflineItem(const std::string& id,
                                   const std::string& name_space) = 0;

    // Called to show all available offline content.
    virtual void LaunchDownloadsPage() = 0;

   protected:
    Delegate();
    virtual ~Delegate();

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Will invoke methods on |delegate| in response to user actions taken on the
  // error page. May call delegate methods even after the page has been
  // navigated away from, so it is recommended consumers make sure the weak
  // pointers are destroyed in response to navigations.
  static void Install(content::RenderFrame* render_frame,
                      base::WeakPtr<Delegate> delegate);

 private:
  explicit NetErrorPageController(base::WeakPtr<Delegate> delegate);
  ~NetErrorPageController() override;

  // Execute a "Show saved copy" button click.
  bool ShowSavedCopyButtonClick();

  // Execute a button click to download page later.
  bool DownloadButtonClick();

  // Execute a "Reload" button click.
  bool ReloadButtonClick();

  // Execute a "Details" button click.
  bool DetailsButtonClick();

  // Track easter egg plays.
  bool TrackEasterEgg();

  // Execute a "Diagnose Errors" button click.
  bool DiagnoseErrorsButtonClick();

  // Track "Show cached copy" button clicks.
  bool TrackCachedCopyButtonClick();

  // Track a click when the page has suggestions from the navigation correction
  // service.
  bool TrackClick(const gin::Arguments& args);

  // Used internally by other button click methods.
  bool ButtonClick(NetErrorHelperCore::Button button);

  void LaunchOfflineItem(gin::Arguments* args);
  void LaunchDownloadsPage();

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  base::WeakPtr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorPageController);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_PAGE_CONTROLLER_H_
