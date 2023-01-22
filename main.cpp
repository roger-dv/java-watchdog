#include <string_view>
#include <cstring>
#include <sys/file.h>
#include <sys/stat.h>
#include "decl-exception.h"
#include "format2str.h"
#include "path-concat.h"
#include "cfgparse.h"
#include "log.h"

//#undef NDEBUG // uncomment this line to enable asserts in use below
#include <cassert>
#ifdef NDEBUG
void __assert (const char *__assertion, const char *__file, int __line) __THROW {}
#endif

using namespace logger;
using std::string_view;

DECL_EXCEPTION(find_program_path)

static int s_parent_thrd_pid = 0;
static string_view s_progpath;
static string_view s_progname;
inline int get_parent_pid() { return s_parent_thrd_pid; }
const char* progpath() { return s_progpath.data(); }
const char* progname() { return s_progname.data(); }


static std::string get_env_var(const char * const name) {
  char * const val = getenv(name);
  auto rtn_str( val != nullptr ? std::string(val) : std::string() );
  return rtn_str;
}

enum class ACCEPT_ORDINAL : char {
  FIRST_FOUND = 0,
  SECOND_FOUND,
  THIRD_FOUND,
  FOURTH_FOUND,
  FIFTH_FOUND,
  SIXTH_FOUND,
  SEVENTH_FOUND,
  LAST_FOUND = -1,
};
using AO = ACCEPT_ORDINAL;

static std::string find_program_path(const char * const prog, const char * const path_var_name) {
  const std::string path_env_var = get_env_var(path_var_name);

  if (path_env_var.empty()) {
    const char * const err_msg_fmt = "there is no %s environment variable defined";
    throw find_program_path_exception(format2str(err_msg_fmt, path_var_name));
  }

  const char * const path_env_var_dup = strdupa(path_env_var.c_str());

  static const char * const delim = ":";
  char *save = nullptr;
  const char * path = strtok_r(const_cast<char*>(path_env_var_dup), delim, &save);

  while (path != nullptr) {
    log(LL::TRACE, "'%s'", path);
    const string_view sv_path{path};
    const auto full_path = sv_path.ends_with(kPathSeparator) ?
        format2str("%s%s", path, prog) : format2str("%s%c%s", path, kPathSeparator, prog);
    log(LL::TRACE, "'%s'", full_path.c_str());
    // check to see if program file path exist
    struct stat statbuf{0};
    if (stat(full_path.c_str(), &statbuf) != -1 &&
        ((statbuf.st_mode & S_IFMT) == S_IFREG || (statbuf.st_mode & S_IFMT) == S_IFLNK))
    {
      return full_path;
    }
    path = strtok_r(nullptr, delim, &save);
  }

  const char * const err_msg_fmt = "could not locate program '%s' via %s environment variable";
  throw find_program_path_exception(format2str(err_msg_fmt, prog, path_var_name));
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"
#pragma ide diagnostic ignored "UnreachableCode"
static void one_time_init_main(int argc, const char *argv[])
{
  s_parent_thrd_pid = getpid();

  const char *malloced_tmpstr = nullptr;

  {
    malloced_tmpstr = strdup(argv[0]);
    if (malloced_tmpstr == nullptr) {
      __assert("strdup() could not duplicate program full path name on startup", __FILE__, __LINE__);
      _exit(EXIT_FAILURE); // will exit here if __assert() defined to no-op function
    }
    s_progpath = malloced_tmpstr;
  }

  {
    malloced_tmpstr = [](const char* const path) -> const char* {
      auto const dup_path = strdupa(path);
      return strdup(basename(dup_path));
    }(progpath());
    if (malloced_tmpstr == nullptr) {
      __assert("strdup() could not duplicate program base name on startup", __FILE__, __LINE__);
      _exit(EXIT_FAILURE); // will exit here if __assert() defined to no-op function
    }
    s_progname = malloced_tmpstr;
  }

  logger::set_progname(progname());
  logger::set_to_unbuffered();
  log(LL::DEBUG, "starting process %d", get_parent_pid());
}
#pragma clang diagnostic pop

int main(int argc, const char *argv[]) {
  set_level(LL::DEBUG); // comment out this line to disable debug logging verbosity
  one_time_init_main(argc, argv);
  LOGGING_LEVEL logging_level = LL::INFO;
  ACCEPT_ORDINAL accept_ordinal = AO::FIRST_FOUND;

  auto prs_cfg_callback =
      [&logging_level, &accept_ordinal](const char *section, const char *name, const char *value_cstr)
  {
    const auto to_lower = [](std::string &str) {
      transform(str.begin(), str.end(), str.begin(), ::tolower);
    };

    std::string s_section{section};
    to_lower(s_section);
    if (s_section.compare("settings")) {
      std::string s_name{name};
      to_lower(s_name);
      std::string s_value{value_cstr};
      to_lower(s_value);
      if (s_name.compare("logging_level")) {
        if (s_value.compare("trace")) {
          logging_level = LL::TRACE;
        } else if (s_value.compare("debug")) {
          logging_level = LL::DEBUG;
        } else if (s_value.compare("info")) {
          logging_level = LL::INFO;
        } else if (s_value.compare("warn")) {
          logging_level = LL::WARN;
        } else if (s_value.compare("err")) {
          logging_level = LL::ERR;
        } else {
          logging_level = LL::INFO;
        }
      } else if (s_name.compare("accept_ordinal")) {
        if (s_value.compare("first_found")) {
          accept_ordinal = AO::FIRST_FOUND;
        } else if (s_value.compare("last_found")) {
          accept_ordinal = AO::LAST_FOUND;
        } else if (s_value.compare("second_found")) {
          accept_ordinal = AO::SECOND_FOUND;
        } else if (s_value.compare("third_found")) {
          accept_ordinal = AO::THIRD_FOUND;
        } else if (s_value.compare("fourth_found")) {
          accept_ordinal = AO::FOURTH_FOUND;
        } else if (s_value.compare("fifth_found")) {
          accept_ordinal = AO::FIFTH_FOUND;
        } else if (s_value.compare("sixth_found")) {
          accept_ordinal = AO::SIXTH_FOUND;
        } else if (s_value.compare("seventh_found")) {
          accept_ordinal = AO::SEVENTH_FOUND;
        } else {
          log(LL::WARN, "unrecognized settings section %s value \"%s\" ignored", name, value_cstr);
          return 1;
        }
      } else {
        log(LL::WARN, "unrecognized settings section name %s ignored", name);
        return 1;
      }
    } else {
      log(LL::WARN, "unrecognized config section %s ignored", section);
      return 1;
    }
    return 0;
  };
  process_config(progpath(), "config.ini", prs_cfg_callback);

  try {
    // determine fully qualified path to the program
    const auto java_prog_path = find_program_path("java", "PATH");
    log(LL::DEBUG, "Java launcher program: \"%s\"", java_prog_path.c_str());
  } catch(const find_program_path_exception &ex) {
    log(LL::ERR, "could not locate a valid Java launcher program - %s: %s", ex.name(), ex.what());
  }
  return 0;
}