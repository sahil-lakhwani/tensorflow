/* Copyright 2020 The TensorFlow Authors All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/core/profiler/rpc/client/profiler_client.h"

#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/platform.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/profiler/lib/profiler_session.h"
#include "tensorflow/core/profiler/profiler_service.pb.h"
#include "tensorflow/core/profiler/rpc/client/profiler_client_test_util.h"
#include "tensorflow/core/profiler/rpc/profiler_server.h"

namespace tensorflow {
namespace profiler {
namespace {

using ::tensorflow::profiler::test::DurationApproxLess;
using ::tensorflow::profiler::test::DurationNear;
using ::tensorflow::profiler::test::StartServer;

TEST(RemoteProfilerSession, Simple) {
  absl::Duration duration = absl::Milliseconds(10);
  ProfileRequest request;
  std::string service_addr;
  auto server = StartServer(duration, &service_addr, &request);
  absl::Duration grace = absl::Seconds(1);
  absl::Duration max_duration = duration + grace;
  absl::Time approx_start = absl::Now();
  absl::Time deadline = approx_start + max_duration;

  auto remote_session =
      RemoteProfilerSession::Create(service_addr, deadline, request);

  Status status;
  auto response = remote_session->WaitForCompletion(status);
  absl::Duration elapsed = absl::Now() - approx_start;
  // At end of session this evaluates to true still.
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(response->empty_trace());
  EXPECT_GT(response->tool_data_size(), 0);
  EXPECT_THAT(elapsed, DurationApproxLess(max_duration));
}

TEST(RemoteProfilerSession, WaitNotCalled) {
  absl::Duration duration = absl::Milliseconds(10);
  ProfileRequest request;
  std::string service_addr;
  auto server = StartServer(duration, &service_addr, &request);
  absl::Duration grace = absl::Seconds(1);
  absl::Duration max_duration = duration + grace;
  absl::Time approx_start = absl::Now();
  absl::Time deadline = approx_start + max_duration;

  auto remote_session =
      RemoteProfilerSession::Create(service_addr, deadline, request);
  absl::Duration elapsed = absl::Now() - approx_start;

  EXPECT_THAT(elapsed, DurationApproxLess(max_duration));
}

TEST(RemoteProfilerSession, Timeout) {
  absl::Duration duration = absl::Milliseconds(10);
  ProfileRequest request;
  std::string service_addr;
  auto server = StartServer(duration, &service_addr, &request);
  // Expect this to fail immediately since deadline was set to the past,
  auto remote_session =
      RemoteProfilerSession::Create(service_addr, absl::Now(), request);
  Status status;
  auto response = remote_session->WaitForCompletion(status);
  // At end of session we will have a timeout error.
  EXPECT_EQ(status.code(), error::DEADLINE_EXCEEDED);

  EXPECT_FALSE(response->empty_trace());  // This defaults to false.
  EXPECT_EQ(response->tool_data_size(), 0);
}

TEST(RemoteProfilerSession, LongDeadline) {
  absl::Duration duration = absl::Milliseconds(10);
  ProfileRequest request;
  std::string service_addr;
  auto server = StartServer(duration, &service_addr, &request);

  absl::Time approx_start = absl::Now();
  absl::Duration grace = absl::Seconds(1000);
  absl::Duration max_duration = duration + grace;
  const absl::Time deadline = approx_start + max_duration;

  auto remote_session =
      RemoteProfilerSession::Create(service_addr, deadline, request);
  Status status;
  auto response = remote_session->WaitForCompletion(status);
  absl::Duration elapsed = absl::Now() - approx_start;
  // At end of session this evaluates to true still.
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(response->empty_trace());
  EXPECT_GT(response->tool_data_size(), 0);
  // Elapsed time is near profiling duration despite long grace period.
  EXPECT_THAT(elapsed, DurationNear(duration));
}

TEST(RemoteProfilerSession, LongDuration) {
  absl::Duration duration = absl::Seconds(3);
  ProfileRequest request;
  std::string service_addr;
  auto server = StartServer(duration, &service_addr, &request);

  absl::Time approx_start = absl::Now();
  // Empirically determined value.
  absl::Duration grace = absl::Seconds(20);
  absl::Duration max_duration = duration + grace;
  const absl::Time deadline = approx_start + max_duration;

  auto remote_session =
      RemoteProfilerSession::Create(service_addr, deadline, request);
  Status status;
  auto response = remote_session->WaitForCompletion(status);
  absl::Duration elapsed = absl::Now() - approx_start;
  // At end of session this evaluates to true still.
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(response->empty_trace());
  EXPECT_GT(response->tool_data_size(), 0);
  // Elapsed time takes longer to complete for larger traces.
  EXPECT_THAT(elapsed, DurationApproxLess(max_duration));
}

}  // namespace
}  // namespace profiler
}  // namespace tensorflow
