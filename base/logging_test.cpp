/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/logging.h"

#include <libgen.h>

#include <regex>
#include <string>

#include "base/file.h"
#include "base/stringprintf.h"
#include "base/test_utils.h"

#include <gtest/gtest.h>

#ifdef __ANDROID__
#define HOST_TEST(suite, name) TEST(suite, DISABLED_ ## name)
#else
#define HOST_TEST(suite, name) TEST(suite, name)
#endif

class CapturedStderr {
 public:
  CapturedStderr() : old_stderr_(-1) {
    init();
  }

  ~CapturedStderr() {
    reset();
  }

  int fd() const {
    return temp_file_.fd;
  }

 private:
  void init() {
    old_stderr_ = dup(STDERR_FILENO);
    ASSERT_NE(-1, old_stderr_);
    ASSERT_NE(-1, dup2(fd(), STDERR_FILENO));
  }

  void reset() {
    ASSERT_NE(-1, dup2(old_stderr_, STDERR_FILENO));
    ASSERT_EQ(0, close(old_stderr_));
  }

  TemporaryFile temp_file_;
  int old_stderr_;
};

TEST(logging, CHECK) {
  ASSERT_DEATH(CHECK(false), "Check failed: false ");
  CHECK(true);

  ASSERT_DEATH(CHECK_EQ(0, 1), "Check failed: 0 == 1 ");
  CHECK_EQ(0, 0);

  ASSERT_DEATH(CHECK_STREQ("foo", "bar"), R"(Check failed: "foo" == "bar")");
  CHECK_STREQ("foo", "foo");
}

std::string make_log_pattern(android::base::LogSeverity severity,
                             const char* message) {
  static const char* log_characters = "VDIWEF";
  char log_char = log_characters[severity];
  std::string holder(__FILE__);
  return android::base::StringPrintf(
      "%c[[:space:]]+[[:digit:]]+[[:space:]]+[[:digit:]]+ %s:[[:digit:]]+] %s",
      log_char, basename(&holder[0]), message);
}

TEST(logging, LOG) {
  ASSERT_DEATH(LOG(FATAL) << "foobar", "foobar");

  // We can't usefully check the output of any of these on Windows because we
  // don't have std::regex, but we can at least make sure we printed at least as
  // many characters are in the log message.
  {
    CapturedStderr cap;
    LOG(WARNING) << "foobar";
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_GT(output.length(), strlen("foobar"));

#if !defined(_WIN32)
    std::regex message_regex(
        make_log_pattern(android::base::WARNING, "foobar"));
    ASSERT_TRUE(std::regex_search(output, message_regex)) << output;
#endif
  }

  {
    CapturedStderr cap;
    LOG(INFO) << "foobar";
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_GT(output.length(), strlen("foobar"));

#if !defined(_WIN32)
    std::regex message_regex(
        make_log_pattern(android::base::INFO, "foobar"));
    ASSERT_TRUE(std::regex_search(output, message_regex)) << output;
#endif
  }

  {
    CapturedStderr cap;
    LOG(DEBUG) << "foobar";
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_TRUE(output.empty());
  }

  {
    android::base::ScopedLogSeverity severity(android::base::DEBUG);
    CapturedStderr cap;
    LOG(DEBUG) << "foobar";
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_GT(output.length(), strlen("foobar"));

#if !defined(_WIN32)
    std::regex message_regex(
        make_log_pattern(android::base::DEBUG, "foobar"));
    ASSERT_TRUE(std::regex_search(output, message_regex)) << output;
#endif
  }
}

TEST(logging, PLOG) {
  {
    CapturedStderr cap;
    errno = ENOENT;
    PLOG(INFO) << "foobar";
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_GT(output.length(), strlen("foobar"));

#if !defined(_WIN32)
    std::regex message_regex(make_log_pattern(
        android::base::INFO, "foobar: No such file or directory"));
    ASSERT_TRUE(std::regex_search(output, message_regex)) << output;
#endif
  }
}

TEST(logging, UNIMPLEMENTED) {
  {
    CapturedStderr cap;
    errno = ENOENT;
    UNIMPLEMENTED(ERROR);
    ASSERT_EQ(0, lseek(cap.fd(), SEEK_SET, 0));

    std::string output;
    android::base::ReadFdToString(cap.fd(), &output);
    ASSERT_GT(output.length(), strlen("unimplemented"));

#if !defined(_WIN32)
    std::string expected_message =
        android::base::StringPrintf("%s unimplemented ", __PRETTY_FUNCTION__);
    std::regex message_regex(
        make_log_pattern(android::base::ERROR, expected_message.c_str()));
    ASSERT_TRUE(std::regex_search(output, message_regex)) << output;
#endif
  }
}
