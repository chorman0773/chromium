// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_ARC_SHARED_SAMPLER_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_ARC_SHARED_SAMPLER_H_

#include <stdint.h>
#include <map>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace task_manager {

// Defines sampler that will retrieve memory footprint metrics for all arc
// processes at once. Created by TaskManagerImpl on the UI thread.
class ArcSharedSampler {
 public:
  ArcSharedSampler();
  ~ArcSharedSampler();

  using MemoryFootprintBytes = uint64_t;

  using OnSamplingCompleteCallback =
      base::RepeatingCallback<void(base::Optional<MemoryFootprintBytes>)>;

  // Registers task group specific callback.
  void RegisterCallback(base::ProcessId process_id,
                        OnSamplingCompleteCallback on_sampling_complete);
  // Unregisters task group specific callbacks.
  void UnregisterCallback(base::ProcessId process_id);

  // Triggers a refresh of process stats.
  void Refresh();

 private:
  using CallbacksMap =
      base::flat_map<base::ProcessId, OnSamplingCompleteCallback>;

  // Called when ArcProcessService returns memory dump.
  void OnReceiveMemoryDump(
      int dump_type,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // Holds callbacks registered by TaskGroup objects.
  CallbacksMap callbacks_;

  // Keeps track of whether there is a pending request for memory footprint of
  // app or system processes.
  int pending_memory_dump_types_ = 0;

  base::WeakPtrFactory<ArcSharedSampler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSharedSampler);
};

}  // namespace task_manager
#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_ARC_SHARED_SAMPLER_H_
