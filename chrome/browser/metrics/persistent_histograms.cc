// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/persistent_histograms.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Creating a "spare" file for persistent metrics involves a lot of I/O and
// isn't important so delay the operation for a while after startup.
#if defined(OS_ANDROID)
// Android needs the spare file and also launches faster.
constexpr bool kSpareFileRequired = true;
constexpr int kSpareFileCreateDelaySeconds = 10;
#else
// Desktop may have to restore a lot of tabs so give it more time before doing
// non-essential work. The spare file is still a performance boost but not as
// significant of one so it's not required.
constexpr bool kSpareFileRequired = false;
constexpr int kSpareFileCreateDelaySeconds = 90;
#endif

}  // namespace

const char kBrowserMetricsName[] = "BrowserMetrics";

// Check for feature enabling the use of persistent histogram storage and
// enable the global allocator if so.
void InstantiatePersistentHistograms() {
  base::FilePath metrics_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &metrics_dir))
    return;

  // Create a directory for storing completed metrics files. Files in this
  // directory must have embedded system profiles. If the directory can't be
  // created, the file will just be deleted below.
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  base::CreateDirectory(upload_dir);

  // Metrics files are typically created as a |spare_file| in the profile
  // directory (e.g. "BrowserMetrics-spare.pma") and are then rotated into
  // a subdirectory as a stamped file for upload when no longer in use.
  // (e.g. "BrowserMetrics/BrowserMetrics-1234ABCD-12345.pma")
  base::FilePath upload_file;
  base::FilePath active_file;
  base::FilePath spare_file;
  base::GlobalHistogramAllocator::ConstructFilePathsForUploadDir(
      metrics_dir, upload_dir, kBrowserMetricsName, &upload_file, &active_file,
      &spare_file);

  // This is used to report results to an UMA histogram.
  enum InitResult {
    kLocalMemorySuccess,
    kLocalMemoryFailed,
    kMappedFileSuccess,
    kMappedFileFailed,
    kMappedFileExists,
    kNoSpareFile,
    kNoUploadDir,
    kMaxValue = kNoUploadDir
  };
  InitResult result;

  // Create persistent/shared memory and allow histograms to be stored in
  // it. Memory that is not actualy used won't be physically mapped by the
  // system. BrowserMetrics usage, as reported in UMA, has the 99.99
  // percentile around 3MiB as of 2018-10-22.
  const size_t kAllocSize = 4 << 20;     // 4 MiB
  const uint32_t kAllocId = 0x935DDD43;  // SHA1(BrowserMetrics)
  std::string storage = variations::GetVariationParamValueByFeature(
      base::kPersistentHistogramsFeature, "storage");

  static const char kMappedFile[] = "MappedFile";
  static const char kLocalMemory[] = "LocalMemory";

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Linux kernel 4.4.0.* shows a huge number of SIGBUS crashes with persistent
  // histograms enabled using a mapped file.  Change this to use local memory.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=753741
  if (storage.empty() || storage == kMappedFile) {
    int major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    if (major == 4 && minor == 4 && bugfix == 0)
      storage = kLocalMemory;
  }
#endif

  // Don't use mapped-file memory by default on low-end devices, especially
  // Android. The extra disk consumption and/or extra disk access could have
  // a significant performance impact. https://crbug.com/896394
  if (storage.empty() && base::SysInfo::IsLowEndDevice())
    storage = kLocalMemory;

  // Create a global histogram allocator using the desired storage type.
  if (storage.empty() || storage == kMappedFile) {
    if (!base::PathExists(upload_dir)) {
      // Handle failure to create the directory.
      result = kNoUploadDir;
    } else if (base::PathExists(upload_file)) {
      // "upload" filename is supposed to be unique so this shouldn't happen.
      result = kMappedFileExists;
    } else {
      // Move any sparse file into the upload position.
      base::ReplaceFile(spare_file, upload_file, nullptr);
      // Create global allocator using the "upload" file.
      if (kSpareFileRequired && !base::PathExists(upload_file)) {
        result = kNoSpareFile;
      } else if (base::GlobalHistogramAllocator::CreateWithFile(
                     upload_file, kAllocSize, kAllocId, kBrowserMetricsName)) {
        result = kMappedFileSuccess;
      } else {
        result = kMappedFileFailed;
      }
    }
    // Schedule the creation of a "spare" file for use on the next run.
    base::PostDelayedTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::LOWEST,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(
                           &base::GlobalHistogramAllocator::CreateSpareFile),
                       std::move(spare_file), kAllocSize),
        base::TimeDelta::FromSeconds(kSpareFileCreateDelaySeconds));
  } else if (storage == kLocalMemory) {
    // Use local memory for storage even though it will not persist across
    // an unclean shutdown. This sets the result but the actual creation is
    // done below.
    result = kLocalMemorySuccess;
  } else {
    // Persistent metric storage is disabled. Must return here.
    return;
  }

  // Get the allocator that was just created and report result. Exit if the
  // allocator could not be created.
  UMA_HISTOGRAM_ENUMERATION("UMA.PersistentHistograms.InitResult", result);

  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator) {
    // If no allocator was created above, try to create a LocalMemomory one
    // here. This avoids repeating the call many times above. In the case where
    // persistence is disabled, an early return is done above.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(kAllocSize, kAllocId,
                                                          kBrowserMetricsName);
    allocator = base::GlobalHistogramAllocator::Get();
    if (!allocator)
      return;
  }

  // Store a copy of the system profile in this allocator.
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->RegisterPersistentAllocator(allocator->memory_allocator());

  // Create tracking histograms for the allocator and record storage file.
  allocator->CreateTrackingHistograms(kBrowserMetricsName);
}
