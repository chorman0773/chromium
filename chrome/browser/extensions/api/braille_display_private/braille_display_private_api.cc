// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"

#include <utility>

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

namespace OnDisplayStateChanged =
    extensions::api::braille_display_private::OnDisplayStateChanged;
namespace OnKeyEvent = extensions::api::braille_display_private::OnKeyEvent;
namespace WriteDots = extensions::api::braille_display_private::WriteDots;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::BrailleController;

namespace extensions {

class BrailleDisplayPrivateAPI::DefaultEventDelegate
    : public BrailleDisplayPrivateAPI::EventDelegate {
 public:
  DefaultEventDelegate(EventRouter::Observer* observer, Profile* profile);
  ~DefaultEventDelegate() override;

  void BroadcastEvent(std::unique_ptr<Event> event) override;
  bool HasListener() override;

 private:
  EventRouter::Observer* observer_;
  Profile* profile_;
};

BrailleDisplayPrivateAPI::BrailleDisplayPrivateAPI(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      scoped_observer_(this),
      event_delegate_(new DefaultEventDelegate(this, profile_)) {}

BrailleDisplayPrivateAPI::~BrailleDisplayPrivateAPI() {
}

void BrailleDisplayPrivateAPI::Shutdown() {
  event_delegate_.reset();
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BrailleDisplayPrivateAPI>>::DestructorAtExit
    g_braille_display_private_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BrailleDisplayPrivateAPI>*
BrailleDisplayPrivateAPI::GetFactoryInstance() {
  return g_braille_display_private_api_factory.Pointer();
}

void BrailleDisplayPrivateAPI::OnBrailleDisplayStateChanged(
    const DisplayState& display_state) {
  std::unique_ptr<Event> event(
      new Event(events::BRAILLE_DISPLAY_PRIVATE_ON_DISPLAY_STATE_CHANGED,
                OnDisplayStateChanged::kEventName,
                OnDisplayStateChanged::Create(display_state)));
  event_delegate_->BroadcastEvent(std::move(event));
}

void BrailleDisplayPrivateAPI::OnBrailleKeyEvent(const KeyEvent& key_event) {
  // Key events only go to extensions of the active profile.
  if (!IsProfileActive())
    return;
  std::unique_ptr<Event> event(
      new Event(events::BRAILLE_DISPLAY_PRIVATE_ON_KEY_EVENT,
                OnKeyEvent::kEventName, OnKeyEvent::Create(key_event)));
  event_delegate_->BroadcastEvent(std::move(event));
}

bool BrailleDisplayPrivateAPI::IsProfileActive() {
#if defined(OS_CHROMEOS)
  Profile* active_profile;
  chromeos::ScreenLocker* screen_locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (screen_locker && screen_locker->locked()) {
    active_profile = chromeos::ProfileHelper::GetSigninProfile();
  } else {
    // Since we are creating one instance per profile / user, we should be fine
    // comparing against the active user. That said - if we ever change that,
    // this code will need to be changed.
    active_profile = ProfileManager::GetActiveUserProfile();
  }
  return profile_->IsSameProfile(active_profile);
#else  // !defined(OS_CHROMEOS)
  return true;
#endif
}

void BrailleDisplayPrivateAPI::SetEventDelegateForTest(
    std::unique_ptr<EventDelegate> delegate) {
  event_delegate_ = std::move(delegate);
}

void BrailleDisplayPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  BrailleController* braille_controller = BrailleController::GetInstance();
  if (!scoped_observer_.IsObserving(braille_controller))
    scoped_observer_.Add(braille_controller);
}

void BrailleDisplayPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  BrailleController* braille_controller = BrailleController::GetInstance();
  if (!event_delegate_->HasListener() &&
      scoped_observer_.IsObserving(braille_controller)) {
    scoped_observer_.Remove(braille_controller);
  }
}

BrailleDisplayPrivateAPI::DefaultEventDelegate::DefaultEventDelegate(
    EventRouter::Observer* observer, Profile* profile)
    : observer_(observer), profile_(profile) {
  EventRouter* event_router = EventRouter::Get(profile_);
  event_router->RegisterObserver(observer_, OnDisplayStateChanged::kEventName);
  event_router->RegisterObserver(observer_, OnKeyEvent::kEventName);
}

BrailleDisplayPrivateAPI::DefaultEventDelegate::~DefaultEventDelegate() {
  EventRouter::Get(profile_)->UnregisterObserver(observer_);
}

void BrailleDisplayPrivateAPI::DefaultEventDelegate::BroadcastEvent(
    std::unique_ptr<Event> event) {
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

bool BrailleDisplayPrivateAPI::DefaultEventDelegate::HasListener() {
  EventRouter* event_router = EventRouter::Get(profile_);
  return (event_router->HasEventListener(OnDisplayStateChanged::kEventName) ||
          event_router->HasEventListener(OnKeyEvent::kEventName));
}

namespace api {
bool BrailleDisplayPrivateGetDisplayStateFunction::Prepare() {
  return true;
}

void BrailleDisplayPrivateGetDisplayStateFunction::Work() {
  SetResult(BrailleController::GetInstance()->GetDisplayState()->ToValue());
}

bool BrailleDisplayPrivateGetDisplayStateFunction::Respond() {
  return true;
}

BrailleDisplayPrivateWriteDotsFunction::
BrailleDisplayPrivateWriteDotsFunction() {
}

BrailleDisplayPrivateWriteDotsFunction::
~BrailleDisplayPrivateWriteDotsFunction() {
}

bool BrailleDisplayPrivateWriteDotsFunction::Prepare() {
  params_ = WriteDots::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);
  EXTENSION_FUNCTION_VALIDATE(
      params_->cells.size() >=
      static_cast<size_t>(params_->columns * params_->rows));
  return true;
}

void BrailleDisplayPrivateWriteDotsFunction::Work() {
  BrailleController::GetInstance()->WriteDots(params_->cells, params_->columns,
                                              params_->rows);
}

bool BrailleDisplayPrivateWriteDotsFunction::Respond() {
  return true;
}

ExtensionFunction::ResponseAction
BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction::Run() {
#if !defined(OS_CHROMEOS)
  NOTREACHED();
  return RespondNow(Error("Unsupported on this platform."));
#else
  std::string address;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &address));
  chromeos::AccessibilityManager::Get()->UpdateBluetoothBrailleDisplayAddress(
      address);
  return RespondNow(NoArguments());
#endif
}

}  // namespace api
}  // namespace extensions
