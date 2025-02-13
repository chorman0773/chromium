// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_

#include <cstddef>

#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {
namespace vector_math {
namespace sse {

constexpr size_t kBitsPerRegister = 128u;
constexpr size_t kPackedFloatsPerRegister = kBitsPerRegister / 32u;
constexpr size_t kFramesToProcessMask = ~(kPackedFloatsPerRegister - 1u);

bool IsAligned(const float*);

// Direct vector convolution:
// dest[k] = sum(source[k+m]*filter[m*filter_stride]) for all m
// provided that |prepared_filter_p| is |prepared_filter->Data()| and that
// |prepared_filter| is prepared with |PrepareFilterForConv|.
void Conv(const float* source_p,
          const float* prepared_filter_p,
          float* dest_p,
          size_t frames_to_process,
          size_t filter_size);

void PrepareFilterForConv(const float* filter_p,
                          int filter_stride,
                          size_t filter_size,
                          AudioFloatArray* prepared_filter);

// dest[k] = source1[k] + source2[k]
void Vadd(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process);

// dest[k] = clip(source[k], low_threshold, high_threshold)
//         = max(low_threshold, min(high_threshold, source[k]))
void Vclip(const float* source_p,
           const float* low_threshold_p,
           const float* high_threshold_p,
           float* dest_p,
           size_t frames_to_process);

// *max_p = max(*max_p, source_max) where
// source_max = max(abs(source[k])) for all k
void Vmaxmgv(const float* source_p, float* max_p, size_t frames_to_process);

// dest[k] = source1[k] * source2[k]
void Vmul(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process);

// dest[k] += scale * source[k]
void Vsma(const float* source_p,
          const float* scale,
          float* dest_p,
          size_t frames_to_process);

// dest[k] = scale * source[k]
void Vsmul(const float* source_p,
           const float* scale,
           float* dest_p,
           size_t frames_to_process);

// sum += sum(source[k]^2) for all k
void Vsvesq(const float* source_p, float* sum_p, size_t frames_to_process);

// real_dest[k] = real1[k] * real2[k] - imag1[k] * imag2[k]
// imag_dest[k] = real1[k] * imag2[k] + imag1[k] * real2[k]
void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process);

}  // namespace sse
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_SSE_H_
