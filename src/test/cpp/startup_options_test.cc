// Copyright 2016 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/main/cpp/startup_options.h"

#include <stdlib.h>

#include "src/main/cpp/blaze_util_platform.h"
#include "src/main/cpp/workspace_layout.h"
#include "src/test/cpp/test_util.h"
#include "googletest/include/gtest/gtest.h"

namespace blaze {

class StartupOptionsTest : public ::testing::Test {
 protected:
  StartupOptionsTest() : workspace_layout_(new WorkspaceLayout()) {}
  ~StartupOptionsTest() = default;

  void SetUp() override {
    // This knowingly ignores the possibility of these environment variables
    // being unset because we expect our test runner to set them in all cases.
    // Otherwise, we'll crash here, but this keeps our code simpler.
    old_home_ = GetHomeDir();
    old_test_tmpdir_ = GetEnv("TEST_TMPDIR");

    ReinitStartupOptions();
  }

  void TearDown() override {
    SetEnv("HOME", old_home_);
    SetEnv("TEST_TMPDIR", old_test_tmpdir_);
  }

  // Recreates startup_options_ after changes to the environment.
  void ReinitStartupOptions() {
    startup_options_.reset(new StartupOptions(workspace_layout_.get()));
  }

 private:
  std::unique_ptr<WorkspaceLayout> workspace_layout_;

 protected:
  std::unique_ptr<StartupOptions> startup_options_;

 private:
  std::string old_home_;
  std::string old_test_tmpdir_;
};

TEST_F(StartupOptionsTest, ProductName) {
  ASSERT_EQ("Bazel", startup_options_->product_name);
}

TEST_F(StartupOptionsTest, JavaLoggingOptions) {
  ASSERT_EQ("com.google.devtools.build.lib.util.SingleLineFormatter",
      startup_options_->java_logging_formatter);
}

// TODO(bazel-team): remove the ifdef guard once the implementation of
// GetOutputRoot is stable among the different platforms.
#ifdef __linux
TEST_F(StartupOptionsTest, OutputRootPreferTestTmpdirIfSet) {
  SetEnv("HOME", "/nonexistent/home");
  SetEnv("TEST_TMPDIR", "/nonexistent/tmpdir");
  ReinitStartupOptions();

  ASSERT_EQ("/nonexistent/tmpdir", startup_options_->output_root);
}

TEST_F(StartupOptionsTest, OutputRootUseHomeDirectory) {
  SetEnv("HOME", "/nonexistent/home");
  UnsetEnv("TEST_TMPDIR");
  ReinitStartupOptions();

  ASSERT_EQ("/nonexistent/home/.cache/bazel", startup_options_->output_root);
}
#endif  // __linux

TEST_F(StartupOptionsTest, EmptyFlagsAreInvalidTest) {
  EXPECT_FALSE(startup_options_->IsNullary(""));
  EXPECT_FALSE(startup_options_->IsNullary("--"));
  EXPECT_FALSE(startup_options_->IsUnary(""));
  EXPECT_FALSE(startup_options_->IsUnary("--"));
}

TEST_F(StartupOptionsTest, ValidStartupFlagsTest) {
  // IMPORTANT: Before modifying this test, please contact a Bazel core team
  // member that knows the Google-internal procedure for adding/deprecating
  // startup flags.
  const StartupOptions* options = startup_options_.get();
  ExpectIsNullaryOption(options, "batch");
  ExpectIsNullaryOption(options, "batch_cpu_scheduling");
  ExpectIsNullaryOption(options, "block_for_lock");
  ExpectIsNullaryOption(options, "client_debug");
  ExpectIsNullaryOption(options, "deep_execroot");
  ExpectIsNullaryOption(options, "experimental_oom_more_eagerly");
  ExpectIsNullaryOption(options, "fatal_event_bus_exceptions");
  ExpectIsNullaryOption(options, "host_jvm_debug");
  ExpectIsNullaryOption(options, "master_bazelrc");
  ExpectIsNullaryOption(options, "master_blazerc");
  ExpectIsNullaryOption(options, "watchfs");
  ExpectIsNullaryOption(options, "write_command_log");
  ExpectIsUnaryOption(options, "bazelrc");
  ExpectIsUnaryOption(options, "blazerc");
  ExpectIsUnaryOption(options, "command_port");
  ExpectIsUnaryOption(options, "connect_timeout_secs");
  ExpectIsUnaryOption(options, "experimental_oom_more_eagerly_threshold");
  ExpectIsUnaryOption(options, "host_javabase");
  ExpectIsUnaryOption(options, "host_jvm_args");
  ExpectIsUnaryOption(options, "host_jvm_profile");
  ExpectIsUnaryOption(options, "invocation_policy");
  ExpectIsUnaryOption(options, "io_nice_level");
  ExpectIsUnaryOption(options, "install_base");
  ExpectIsUnaryOption(options, "max_idle_secs");
  ExpectIsUnaryOption(options, "output_base");
  ExpectIsUnaryOption(options, "output_user_root");
}

TEST_F(StartupOptionsTest, ProcessSpaceSeparatedArgsTest) {
  std::string error;
  const std::vector<RcStartupFlag> flags{
      RcStartupFlag("somewhere", "--max_idle_secs"),
      RcStartupFlag("somewhere", "42")};

  const blaze_exit_code::ExitCode ec =
      startup_options_->ProcessArgs(flags, &error);
  ASSERT_EQ(blaze_exit_code::SUCCESS, ec)
      << "ProcessArgs failed with error " << error;
  EXPECT_EQ(42, startup_options_->max_idle_secs);

  EXPECT_EQ("somewhere", startup_options_->original_startup_options_[0].source);
  EXPECT_EQ("--max_idle_secs=42",
            startup_options_->original_startup_options_[0].value);
}

TEST_F(StartupOptionsTest, ProcessEqualsSeparatedArgsTest) {
  std::string error;
  const std::vector<RcStartupFlag> flags{
      RcStartupFlag("somewhere", "--max_idle_secs=36")};

  const blaze_exit_code::ExitCode ec =
      startup_options_->ProcessArgs(flags, &error);
  ASSERT_EQ(ec, blaze_exit_code::SUCCESS)
      << "ProcessArgs failed with error " << error;
  EXPECT_EQ(36, startup_options_->max_idle_secs);

  EXPECT_EQ("somewhere", startup_options_->original_startup_options_[0].source);
  EXPECT_EQ("--max_idle_secs=36",
            startup_options_->original_startup_options_[0].value);
}

TEST_F(StartupOptionsTest, ProcessIncorrectArgValueTest) {
  std::string error;
  const std::vector<RcStartupFlag> flags{
      RcStartupFlag("somewhere", "--max_idle_secs=notANumber")};

  const blaze_exit_code::ExitCode ec =
      startup_options_->ProcessArgs(flags, &error);
  ASSERT_EQ(blaze_exit_code::BAD_ARGV, ec)
      << "ProcessArgs failed with the wrong error " << error;

  // Even for a failing args processing step, expect the original value
  // to be stored.
  EXPECT_EQ("somewhere", startup_options_->original_startup_options_[0].source);
  EXPECT_EQ("--max_idle_secs=notANumber",
            startup_options_->original_startup_options_[0].value);
}

TEST_F(StartupOptionsTest, ProcessArgsWithMultipleArgstest) {
  const std::vector<RcStartupFlag> flags{
      RcStartupFlag("somewhere", "--max_idle_secs=36"),
      RcStartupFlag("somewhereElse", "--nowrite_command_log")};

  std::string error;
  const blaze_exit_code::ExitCode ec =
      startup_options_->ProcessArgs(flags, &error);
  ASSERT_EQ(ec, blaze_exit_code::SUCCESS)
      << "ProcessArgs failed with error " << error;
  EXPECT_EQ(36, startup_options_->max_idle_secs);
  EXPECT_FALSE(startup_options_->write_command_log);

  EXPECT_EQ("somewhere", startup_options_->original_startup_options_[0].source);
  EXPECT_EQ("--max_idle_secs=36",
            startup_options_->original_startup_options_[0].value);

  EXPECT_EQ("somewhereElse",
            startup_options_->original_startup_options_[1].source);
  EXPECT_EQ("--nowrite_command_log",
            startup_options_->original_startup_options_[1].value);
}

}  // namespace blaze
