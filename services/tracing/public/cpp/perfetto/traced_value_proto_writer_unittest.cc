// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"

#include <memory>
#include <string>

#include "base/trace_event/trace_event_argument.h"
#include "services/tracing/public/cpp/perfetto/heap_scattered_stream_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"

using TracedValue = base::trace_event::TracedValue;

namespace tracing {

namespace {

class ProtoInputStream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit ProtoInputStream(HeapScatteredStreamWriterDelegate* delegate)
      : delegate_(delegate) {
    delegate->AdjustUsedSizeOfCurrentChunk();
  }

  // google::protobuf::io::ZeroCopyInputStream implementation.
  bool Next(const void** data, int* size) override {
    DCHECK(!has_backed_up_);

    auto& chunks = delegate_->chunks();
    if (chunks.size() <= chunks_read_) {
      return false;
    }

    *data = chunks[chunks_read_].start();
    *size = chunks[chunks_read_].size() - chunks[chunks_read_].unused_bytes();

    chunks_read_++;
    return true;
  }

  void BackUp(int count) override {
    // This should only be called once, on the last buffer (and
    // that means we can ignore it).
    DCHECK(!has_backed_up_);
    has_backed_up_ = true;
  }

  bool Skip(int count) override {
    NOTREACHED();
    return false;
  }

  int64_t ByteCount() const override {
    NOTREACHED();
    return 0;
  }

 private:
  const HeapScatteredStreamWriterDelegate* delegate_;
  size_t chunks_read_ = 0;
  bool has_backed_up_ = false;
};

class TracedValueProtoWriterTest : public testing::Test {
 public:
  void SetUp() override { RegisterTracedValueProtoWriter(true); }

  void TearDown() override { RegisterTracedValueProtoWriter(false); }
};

const perfetto::protos::ChromeTracedValue* FindDictEntry(
    const perfetto::protos::ChromeTracedValue* dict,
    const char* name) {
  EXPECT_EQ(dict->dict_values_size(), dict->dict_keys_size());

  for (int i = 0; i < dict->dict_keys_size(); ++i) {
    if (dict->dict_keys(i) == name) {
      return &dict->dict_values(i);
    }
  }

  NOTREACHED();
  return nullptr;
}

bool IsValue(const perfetto::protos::ChromeTracedValue* proto_value,
             bool value) {
  return proto_value->has_bool_value() && (proto_value->bool_value() == value);
}

bool IsValue(const perfetto::protos::ChromeTracedValue* proto_value,
             double value) {
  return proto_value->has_double_value() &&
         (proto_value->double_value() == value);
}

bool IsValue(const perfetto::protos::ChromeTracedValue* proto_value,
             int value) {
  return proto_value->has_int_value() && (proto_value->int_value() == value);
}

bool IsValue(const perfetto::protos::ChromeTracedValue* proto_value,
             const char* value) {
  return proto_value->has_string_value() &&
         (proto_value->string_value() == value);
}

perfetto::protos::ChromeTracedValue GetProtoFromTracedValue(
    TracedValue* traced_value) {
  HeapScatteredStreamWriterDelegate delegate_(100);
  protozero::ScatteredStreamWriter stream_(&delegate_);
  perfetto::protos::pbzero::ChromeTraceEvent_Arg proto;
  proto.Reset(&stream_);
  delegate_.set_writer(&stream_);

  PerfettoProtoAppender proto_appender(&proto);
  EXPECT_TRUE(traced_value->AppendToProto(&proto_appender));
  uint32_t size = proto.Finalize();
  ProtoInputStream stream(&delegate_);

  perfetto::protos::ChromeTraceEvent_Arg full_proto;
  EXPECT_TRUE(full_proto.ParseFromBoundedZeroCopyStream(&stream, size));
  EXPECT_TRUE(full_proto.has_traced_value());

  return full_proto.traced_value();
}

TEST_F(TracedValueProtoWriterTest, FlatDictionary) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetBoolean("bool", true);
  value->SetDouble("double", 0.0);
  value->SetInteger("int", 2014);
  value->SetString("string", "string");

  auto full_proto = GetProtoFromTracedValue(value.get());
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "bool"), true));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "double"), 0.0));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "int"), 2014));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "string"), "string"));
}

TEST_F(TracedValueProtoWriterTest, NoDotPathExpansion) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetBoolean("bo.ol", true);
  value->SetDouble("doub.le", 0.0);
  value->SetInteger("in.t", 2014);
  value->SetString("str.ing", "str.ing");

  auto full_proto = GetProtoFromTracedValue(value.get());
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "bo.ol"), true));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "doub.le"), 0.0));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "in.t"), 2014));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "str.ing"), "str.ing"));
}

TEST_F(TracedValueProtoWriterTest, Hierarchy) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->BeginArray("a1");
  value->AppendInteger(1);
  value->AppendBoolean(true);
  value->BeginDictionary();
  value->SetInteger("i2", 3);
  value->EndDictionary();
  value->EndArray();
  value->SetBoolean("b0", true);
  value->SetDouble("d0", 0.0);
  value->BeginDictionary("dict1");
  value->BeginDictionary("dict2");
  value->SetBoolean("b2", false);
  value->EndDictionary();
  value->SetInteger("i1", 2014);
  value->SetString("s1", "foo");
  value->EndDictionary();
  value->SetInteger("i0", 2014);
  value->SetString("s0", "foo");

  auto full_proto = GetProtoFromTracedValue(value.get());

  auto* a1_array = FindDictEntry(&full_proto, "a1");
  EXPECT_TRUE(a1_array);
  EXPECT_EQ(a1_array->nested_type(),
            perfetto::protos::ChromeTracedValue::ARRAY);
  EXPECT_EQ(a1_array->array_values_size(), 3);
  EXPECT_TRUE(IsValue(&a1_array->array_values(0), 1));
  EXPECT_TRUE(IsValue(&a1_array->array_values(1), true));
  auto* a1_subdict = &a1_array->array_values(2);
  EXPECT_TRUE(a1_subdict);
  EXPECT_EQ(a1_subdict->nested_type(),
            perfetto::protos::ChromeTracedValue::DICT);
  EXPECT_EQ(a1_subdict->dict_values_size(), 1);
  EXPECT_TRUE(IsValue(FindDictEntry(a1_subdict, "i2"), 3));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "b0"), true));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "d0"), 0.0));
  auto* dict1 = FindDictEntry(&full_proto, "dict1");
  EXPECT_TRUE(dict1);
  EXPECT_EQ(dict1->dict_values_size(), 3);
  EXPECT_EQ(dict1->nested_type(), perfetto::protos::ChromeTracedValue::DICT);
  auto* dict2 = FindDictEntry(dict1, "dict2");
  EXPECT_TRUE(dict2);
  EXPECT_EQ(dict2->dict_values_size(), 1);
  EXPECT_EQ(dict2->nested_type(), perfetto::protos::ChromeTracedValue::DICT);
  EXPECT_TRUE(IsValue(FindDictEntry(dict2, "b2"), false));
  EXPECT_TRUE(IsValue(FindDictEntry(dict1, "i1"), 2014));
  EXPECT_TRUE(IsValue(FindDictEntry(dict1, "s1"), "foo"));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "i0"), 2014));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "s0"), "foo"));
}

TEST_F(TracedValueProtoWriterTest, LongStrings) {
  std::string kLongString = "supercalifragilisticexpialidocious";
  std::string kLongString2 = "0123456789012345678901234567890123456789";
  char kLongString3[4096];
  for (size_t i = 0; i < sizeof(kLongString3); ++i)
    kLongString3[i] = 'a' + (i % 25);
  kLongString3[sizeof(kLongString3) - 1] = '\0';

  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetString("a", "short");
  value->SetString("b", kLongString);
  value->BeginArray("c");
  value->AppendString(kLongString2);
  value->AppendString("");
  value->BeginDictionary();
  value->SetString("a", kLongString3);
  value->EndDictionary();
  value->EndArray();

  auto full_proto = GetProtoFromTracedValue(value.get());

  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "a"), "short"));
  EXPECT_TRUE(IsValue(FindDictEntry(&full_proto, "b"), kLongString.c_str()));
  auto* c_array = FindDictEntry(&full_proto, "c");
  EXPECT_TRUE(c_array);
  EXPECT_EQ(c_array->array_values_size(), 3);
  EXPECT_EQ(c_array->nested_type(), perfetto::protos::ChromeTracedValue::ARRAY);
  EXPECT_TRUE(IsValue(&c_array->array_values(0), kLongString2.c_str()));
  EXPECT_TRUE(IsValue(&c_array->array_values(1), ""));
  auto* c_subdict = &c_array->array_values(2);
  EXPECT_TRUE(c_subdict);
  EXPECT_EQ(c_subdict->dict_values_size(), 1);
  EXPECT_EQ(c_subdict->nested_type(),
            perfetto::protos::ChromeTracedValue::DICT);
  EXPECT_TRUE(IsValue(FindDictEntry(c_subdict, "a"), kLongString3));
}

// Test that the proto which results from the TracedValue is still
// valid regardless of the size of the buffer chunks we provide to
// the allocator, as some buffer sizes will leave unused bytes
// at the end where there isn't enough space for, say, a size field.
// 10-140 bytes tests both buffers being smaller and larger than
// the actual size of the proto.
TEST_F(TracedValueProtoWriterTest, ProtoMessageBoundaries) {
  for (int i = 10; i < 140; ++i) {
    std::unique_ptr<TracedValue> value(new TracedValue(i));

    value->SetString("source", "RendererCompositor");
    value->SetString("thread", "RendererCompositor");
    value->SetString("compile_target", "Chromium");

    auto full_proto = GetProtoFromTracedValue(value.get());

    EXPECT_TRUE(
        IsValue(FindDictEntry(&full_proto, "source"), "RendererCompositor"));
    EXPECT_TRUE(
        IsValue(FindDictEntry(&full_proto, "thread"), "RendererCompositor"));
    EXPECT_TRUE(
        IsValue(FindDictEntry(&full_proto, "compile_target"), "Chromium"));
  }
}

TEST_F(TracedValueProtoWriterTest, PassTracedValue) {
  auto dict_value = std::make_unique<TracedValue>();
  dict_value->SetInteger("a", 1);

  auto nested_dict_value = std::make_unique<TracedValue>();
  nested_dict_value->SetInteger("b", 2);
  nested_dict_value->BeginArray("c");
  nested_dict_value->AppendString("foo");
  nested_dict_value->EndArray();

  dict_value->SetValue("e", nested_dict_value.get());

  {
    // Check the merged result.
    auto parent_proto = GetProtoFromTracedValue(dict_value.get());

    EXPECT_TRUE(IsValue(FindDictEntry(&parent_proto, "a"), 1));

    auto* nested_dict = FindDictEntry(&parent_proto, "e");
    EXPECT_TRUE(nested_dict);
    EXPECT_EQ(nested_dict->dict_values_size(), 2);
    EXPECT_TRUE(IsValue(FindDictEntry(nested_dict, "b"), 2));
    auto* c_array = FindDictEntry(nested_dict, "c");
    EXPECT_TRUE(c_array);
    EXPECT_EQ(c_array->array_values_size(), 1);
    EXPECT_TRUE(IsValue(&c_array->array_values(0), "foo"));
  }

  {
    // Check that the passed nested dict was left untouched.
    auto child_proto = GetProtoFromTracedValue(nested_dict_value.get());
    EXPECT_TRUE(IsValue(FindDictEntry(&child_proto, "b"), 2));
    auto* c_array = FindDictEntry(&child_proto, "c");
    EXPECT_TRUE(c_array);
    EXPECT_EQ(c_array->array_values_size(), 1);
    EXPECT_TRUE(IsValue(&c_array->array_values(0), "foo"));
  }
}

}  // namespace

}  // namespace tracing
