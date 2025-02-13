// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

// These constants reflect service names in test service manifests within the
// default catalog for service_unittests.
const char kTestServiceName[] = "service_manager_unittest";
const char kTestTargetServiceName[] = "service_manager_unittest_target";

constexpr uint32_t kTestSelfPid = 1234;
constexpr uint32_t kTestTargetPid1 = 4567;
constexpr uint32_t kTestTargetPid2 = 8910;

class TestListener : public mojom::ServiceManagerListener {
 public:
  explicit TestListener(mojom::ServiceManagerListenerRequest request)
      : binding_(this, std::move(request)) {}
  ~TestListener() override = default;

  void WaitForInit() { wait_for_init_loop_.Run(); }

  void WaitForServiceStarted(Identity* out_identity, uint32_t* out_pid) {
    wait_for_start_identity_ = out_identity;
    wait_for_start_pid_ = out_pid;
    wait_for_start_loop_.emplace();
    wait_for_start_loop_->Run();
  }

  // mojom::ServiceManagerListener:
  void OnInit(std::vector<mojom::RunningServiceInfoPtr> instances) override {
    wait_for_init_loop_.Quit();
  }
  void OnServiceCreated(mojom::RunningServiceInfoPtr instance) override {}
  void OnServiceStarted(const Identity& identity, uint32_t pid) override {
    if (wait_for_start_loop_)
      wait_for_start_loop_->Quit();
    if (wait_for_start_identity_)
      *wait_for_start_identity_ = identity;
    if (wait_for_start_pid_)
      *wait_for_start_pid_ = pid;
    wait_for_start_identity_ = nullptr;
    wait_for_start_pid_ = nullptr;
  }
  void OnServiceFailedToStart(const Identity& identity) override {}
  void OnServiceStopped(const Identity& identity) override {}
  void OnServicePIDReceived(const Identity& identity, uint32_t pid) override {}

 private:
  mojo::Binding<mojom::ServiceManagerListener> binding_;
  base::RunLoop wait_for_init_loop_;

  base::Optional<base::RunLoop> wait_for_start_loop_;
  Identity* wait_for_start_identity_ = nullptr;
  uint32_t* wait_for_start_pid_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestListener);
};

class TestTargetService : public Service {
 public:
  explicit TestTargetService(mojom::ServiceRequest request)
      : binding_(this, std::move(request)) {}
  ~TestTargetService() override = default;

  Connector* connector() { return binding_.GetConnector(); }

  // Tells the Service Manager this instance wants to die, and waits for ack.
  // When this returns, we can be sure the Service Manager is no longer keeping
  // this instance's Identity reserved and we may reuse it (modulo a new
  // globally unique ID) for another instance.
  void QuitGracefullyAndWait() {
    binding_.RequestClose();
    wait_for_disconnect_loop_.Run();
  }

 private:
  // Service:
  void OnDisconnected() override { wait_for_disconnect_loop_.Quit(); }

  ServiceBinding binding_;
  base::RunLoop wait_for_disconnect_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTargetService);
};

class ServiceManagerListenerTest : public testing::Test, public Service {
 public:
  ServiceManagerListenerTest() : service_manager_(nullptr, nullptr, nullptr) {}
  ~ServiceManagerListenerTest() override = default;

  Connector* connector() { return service_binding_.GetConnector(); }

  void SetUp() override {
    service_binding_.Bind(
        RegisterServiceInstance(kTestServiceName, kTestSelfPid));

    mojom::ServiceManagerPtr service_manager;
    connector()->BindInterface(mojom::kServiceName, &service_manager);

    mojom::ServiceManagerListenerPtr listener_proxy;
    listener_ =
        std::make_unique<TestListener>(mojo::MakeRequest(&listener_proxy));
    service_manager->AddListener(std::move(listener_proxy));
    listener_->WaitForInit();
  }

  mojom::ServiceRequest RegisterServiceInstance(const std::string& service_name,
                                                uint32_t fake_pid) {
    mojom::ServicePtr proxy;
    mojom::ServiceRequest request = mojo::MakeRequest(&proxy);
    mojom::PIDReceiverPtr pid_receiver;
    service_manager_.RegisterService(
        Identity(service_name, kSystemInstanceGroup), std::move(proxy),
        mojo::MakeRequest(&pid_receiver));
    pid_receiver->SetPID(fake_pid);
    return request;
  }

  void WaitForServiceStarted(Identity* out_identity, uint32_t* out_pid) {
    listener_->WaitForServiceStarted(out_identity, out_pid);
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  ServiceManager service_manager_;
  ServiceBinding service_binding_{this};
  std::unique_ptr<TestListener> listener_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerListenerTest);
};

TEST_F(ServiceManagerListenerTest, InstancesHaveUniqueIdentity) {
  TestTargetService target1(
      RegisterServiceInstance(kTestTargetServiceName, kTestTargetPid1));

  Identity identity1;
  uint32_t pid1;
  WaitForServiceStarted(&identity1, &pid1);
  EXPECT_EQ(kTestTargetServiceName, identity1.name());
  ASSERT_TRUE(identity1.globally_unique_id().has_value());
  EXPECT_FALSE(identity1.globally_unique_id()->is_zero());
  EXPECT_EQ(kTestTargetPid1, pid1);

  // We retain a Connector from the first instance before disconnecting it. This
  // keeps some state for the instance alive in the Service Manager and blocks
  // OnInstanceStopped from being broadcast to ServiceManagerListeners, but the
  // instance is no longer reachable and its basic identity can be reused by a
  // new instance.
  std::unique_ptr<Connector> connector = target1.connector()->Clone();
  target1.QuitGracefullyAndWait();

  TestTargetService target2(
      RegisterServiceInstance(kTestTargetServiceName, kTestTargetPid2));

  Identity identity2;
  uint32_t pid2;
  WaitForServiceStarted(&identity2, &pid2);
  EXPECT_EQ(kTestTargetServiceName, identity2.name());
  ASSERT_TRUE(identity2.globally_unique_id().has_value());
  EXPECT_FALSE(identity2.globally_unique_id()->is_zero());
  EXPECT_EQ(kTestTargetPid2, pid2);

  // This is the important part of the test. The globally unique IDs of both
  // instances must differ, even though all other fields may be (and in this
  // case, will be) the same.
  EXPECT_EQ(identity1.name(), identity2.name());
  EXPECT_EQ(identity1.instance_group(), identity2.instance_group());
  EXPECT_EQ(identity1.instance_id(), identity2.instance_id());
  EXPECT_NE(identity1.globally_unique_id().value(),
            identity2.globally_unique_id().value());
}

}  // namespace
}  // namespace service_manager
