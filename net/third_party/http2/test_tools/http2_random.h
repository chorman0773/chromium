// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_HTTP2_TEST_TOOLS_HTTP2_RANDOM_H_
#define NET_THIRD_PARTY_HTTP2_TEST_TOOLS_HTTP2_RANDOM_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

#include "net/third_party/http2/platform/api/http2_string.h"
#include "net/third_party/http2/platform/api/http2_string_piece.h"

namespace http2 {
namespace test {

// The random number generator used for unit tests.  Since the algorithm is
// deterministic and fixed, this can be used to reproduce flakes in the unit
// tests caused by specific random values.
class Http2Random {
 public:
  Http2Random();

  Http2Random(const Http2Random&) = delete;
  Http2Random& operator=(const Http2Random&) = delete;

  // Reproducible random number generation: by using the same key, the same
  // sequence of results is obtained.
  explicit Http2Random(Http2StringPiece key);
  Http2String Key() const;

  void FillRandom(void* buffer, size_t buffer_size);
  Http2String RandString(int length);

  // Returns a random 64-bit value.
  uint64_t Rand64();

  // Return a uniformly distrubted random number in [0, n).
  int32_t Uniform(int32_t n) { return Rand64() % n; }
  // Return a uniformly distrubted random number in [lo, hi).
  int64_t UniformInRange(int64_t lo, int64_t hi) {
    return lo + Rand64() % (hi - lo);
  }
  // Return an integer of logarithmically random scale.
  int32_t Skewed(int32_t max_log) {
    const int32_t base = Rand32() % (max_log + 1);
    const uint32_t mask = ((base < 32) ? (1u << base) : 0u) - 1u;
    return Rand32() & mask;
  }
  // Return a random number in [0, max] range that skews low.
  size_t RandomSizeSkewedLow(size_t max) {
    return std::round(max * std::pow(RandDouble(), 2));
  }

  // Returns a random double between 0 and 1.
  double RandDouble();
  float RandFloat() { return RandDouble(); }

  // Has 1/n chance of returning true.
  bool OneIn(int n) { return Uniform(n) == 0; }

  uint8_t Rand8() { return Rand64(); }
  uint16_t Rand16() { return Rand64(); }
  uint32_t Rand32() { return Rand64(); }

  // Return a random string consisting of the characters from the specified
  // alphabet.
  Http2String RandStringWithAlphabet(int length, Http2StringPiece alphabet);

  // STL UniformRandomNumberGenerator implementation.
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }
  result_type operator()() { return Rand64(); }

 private:
  uint8_t key_[32];
  uint32_t counter_ = 0;
};

}  // namespace test
}  // namespace http2

#endif  // NET_THIRD_PARTY_HTTP2_TEST_TOOLS_HTTP2_RANDOM_H_
