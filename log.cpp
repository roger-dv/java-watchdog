/* log.cpp

Copyright 2015 - 2022 Tideworks Technology
Author: Roger D. Voss

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <string>
#include <functional>
#include <string_view>
#include "log.h"

//#undef NDEBUG // uncomment this line to enable asserts in use below
#include <cassert>

namespace logger {

  const size_t DEFAULT_STRBUF_SIZE = 256;
  const char CNEWLINE = '\n';
  const char CNULLTRM = '\0';
  const LOGGING_LEVEL DEFAULT_LOGGING_LEVEL = LL::INFO;

  static volatile LOGGING_LEVEL loggingLevel = DEFAULT_LOGGING_LEVEL;
  static std::string_view s_progname;
  static bool s_syslogging_enabled = true;
  using call_openlog_t = std::function<void(const std::string_view, const bool)>;
  static call_openlog_t s_call_openlog = [](const std::string_view ident, bool is_enabled) {
    if (is_enabled) {
      openlog(ident.data(), LOG_PID, LOG_DAEMON);
    }
  };
  using call_syslog_t = std::function<void(const std::string_view, const std::string_view)>;
  static call_syslog_t s_syslog = [](const std::string_view level, const std::string_view msg) {
    syslog(LOG_ERR, "%s: %s", level.data(), msg.data());
  };

  // NOTE: this property must be set on the logger namespace subsystem prior to use of its functions
  void set_progname(const std::string_view progname) {
    const char * tmp_prg_name;
    s_progname = tmp_prg_name = strdup(progname.data());
    s_call_openlog(tmp_prg_name, s_syslogging_enabled);
    s_call_openlog = [](const std::string_view ident, bool is_enabled) {};
  }

  void set_syslogging(bool is_syslogging_enabled) {
    s_syslogging_enabled = is_syslogging_enabled;
    const char * const tmp_prg_name = strndupa(s_progname.data(), s_progname.size());
    s_call_openlog(tmp_prg_name, is_syslogging_enabled);
    s_call_openlog = [](const std::string_view ident, bool is_enabled) {};
    if (!is_syslogging_enabled) {
      s_syslog = [](const std::string_view, const std::string_view) {};
    }
  }

  LOGGING_LEVEL get_level() { return loggingLevel; }

  // trim from start
  static inline std::string& ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
  }

  // trim from end
  static inline std::string& rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
  }

  // trim from both ends
  static inline std::string& trim(std::string &s) {
    return ltrim(rtrim(s));
  }

  LOGGING_LEVEL str_to_level(const std::string_view logging_level) {
    std::string log_level{logging_level};
    trim(log_level);
    std::transform(log_level.begin(), log_level.end(), log_level.begin(), ::toupper);
    if (log_level == "TRACE") return LL::TRACE;
    if (log_level == "DEBUG") return LL::DEBUG;
    if (log_level == "INFO")  return LL::INFO;
    if (log_level == "WARN")  return LL::WARN;
    if (log_level == "ERR")   return LL::ERR;
    if (log_level == "FATAL") return LL::FATAL;
    return DEFAULT_LOGGING_LEVEL; // didn't match anything so return default logging level
  }

  void set_level(LOGGING_LEVEL level) {
    loggingLevel = level;
  }

  void set_to_unbuffered() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
  }

  void vlog(LOGGING_LEVEL level, const std::string_view fmt, va_list ap) {
    if ((char) level < (char) loggingLevel) {
      return;
    }

    auto stream = stdout;
    std::string_view level_str = ": ";
    call_syslog_t syslog_it = [](const std::string_view, const std::string_view) {};
    std::string_view syslog_level = "";
    switch (level) {
      case LL::FATAL:
        level_str = ": FATAL: ";
        stream = stderr;
        syslog_it = s_syslog;
        syslog_level = "FATAL";
        break;
      case LL::ERR:
        level_str = ": ERROR: ";
        stream = stderr;
        syslog_it = s_syslog;
        syslog_level = "ERROR";
        break;
      case LL::WARN:
        level_str = ": WARN: ";
        stream = stderr;
        break;
      case LL::INFO:
        level_str = ": INFO: ";
        break;
      case LL::DEBUG:
        level_str = ": DEBUG: ";
        break;
      case LL::TRACE:
        level_str = ": TRACE: ";
        break;
    }

    const auto len = s_progname.size() + level_str.size();
    const auto buf_extra_size = len + sizeof(CNEWLINE) + sizeof(CNULLTRM);
    int buf_size = DEFAULT_STRBUF_SIZE;
    size_t total_buf_size = buf_size + buf_extra_size;
    auto strbuf = (char*) alloca(total_buf_size);
    const size_t msgbuf_size = total_buf_size - buf_extra_size;
    auto n = (int) msgbuf_size;
    va_list parm_copy;
    va_copy(parm_copy, ap);
    {
      strncpy(strbuf, s_progname.data(), s_progname.size());
      strcpy(strbuf + s_progname.size(), level_str.data());
      n = vsnprintf(strbuf + len, (size_t) n, fmt.data(), ap);
      assert(n > 0);
      if (n >= static_cast<int>(msgbuf_size)) {
        total_buf_size = (buf_size = ++n) + buf_extra_size;
        strbuf = (char*) alloca(total_buf_size);
        strncpy(strbuf, s_progname.data(), s_progname.size());
        strcpy(strbuf + s_progname.size(), level_str.data());
        n = vsnprintf(strbuf + len, (size_t) n, fmt.data(), parm_copy);
        assert(n > 0 && n < buf_size);
      }
    }
    va_end(parm_copy);
    n += len;
    strbuf[n++] = CNEWLINE;
    strbuf[n] = CNULLTRM;
    fputs(strbuf, stream);
    syslog_it(syslog_level, strbuf + len);
  }

  void log(LOGGING_LEVEL level, const std::string_view fmt, ...) {
    if ((char) level < (char) loggingLevel) {
      return;
    }

    va_list ap;
    va_start(ap, fmt);
    vlog(level, fmt, ap);
    va_end(ap);
  }

  void logm(LOGGING_LEVEL level, const std::string_view msg) {
    log(level, "%s", msg);
  }
} // namespace logger
