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
using std::string_view_literals::operator ""sv;

DECL_EXCEPTION(find_program_path)

static int s_parent_thrd_pid = 0;
static const auto cfg_file_name = "config.ini"sv;
static std::string_view s_progpath;
static std::string_view s_progname;
inline int get_parent_pid() { return s_parent_thrd_pid; }
const char* progpath() { return s_progpath.data(); }
const char* progname() { return s_progname.data(); }

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

static std::string get_env_var(const char * const name) {
  char * const val = getenv(name);
  return val != nullptr ? std::string(val) : std::string();
}

static std::string find_program_path(const char * const prog, const char * const path_var_name, ACCEPT_ORDINAL ao) {
  const std::string path_env_var = get_env_var(path_var_name);

  if (path_env_var.empty()) {
    const char * const err_msg_fmt = "there is no %s environment variable defined";
    throw find_program_path_exception(format2str(err_msg_fmt, path_var_name));
  }

  const char * const path_env_var_dup = strdupa(path_env_var.c_str());

  static const char * const delim = ":";
  char *save = nullptr;
  const char * path = strtok_r(const_cast<char*>(path_env_var_dup), delim, &save);

  std::string last_found_valid_path;

  for (int i = 0; path != nullptr;) {
    log(LL::TRACE, "'%s'", path);
    const std::string_view sv_path{path};
    const auto full_path = sv_path.ends_with(kPathSeparator) ?
        format2str("%s%s", path, prog) : format2str("%s%c%s", path, kPathSeparator, prog);
    log(LL::TRACE, "'%s'", full_path.c_str());
    // check to see if program file path exist
    struct stat statbuf{0};
    if (stat(full_path.c_str(), &statbuf) != -1 &&
        ((statbuf.st_mode & S_IFMT) == S_IFREG || (statbuf.st_mode & S_IFMT) == S_IFLNK))
    {
      last_found_valid_path = full_path;
      log(LL::DEBUG, "'%s'", last_found_valid_path.c_str());
      bool matches_ao = false;
      switch(++i) {
        case 1:
          matches_ao = ao == AO::FIRST_FOUND;
          break;
        case 2:
          matches_ao = ao == AO::SECOND_FOUND;
          break;
        case 3:
          matches_ao = ao == AO::THIRD_FOUND;
          break;
        case 4:
          matches_ao = ao == AO::FOURTH_FOUND;
          break;
        case 5:
          matches_ao = ao == AO::FIFTH_FOUND;
          break;
        case 6:
          matches_ao = ao == AO::SIXTH_FOUND;
          break;
        case 7:
          matches_ao = ao == AO::SEVENTH_FOUND;
          break;
      }
      if (matches_ao) {
        return last_found_valid_path;
      }
    }
    path = strtok_r(nullptr, delim, &save);
    if (ao == AO::LAST_FOUND && path == nullptr) {
      return last_found_valid_path;
    }
  }

  const char * const err_msg_fmt = "could not locate program '%s' via %s environment variable";
  throw find_program_path_exception(format2str(err_msg_fmt, prog, path_var_name));
}

static std::string locate_cfg_file() {
  std::string dir;
  std::string cfg_file_path;
  struct stat statbuf{};
  for (int check_exist = 3; ; check_exist--) {
    switch(check_exist) {
      case 3: // using HOME directory
        dir = get_env_var("HOME");
        if (dir.empty()) continue;
        dir = path_concat(dir.c_str(), ".config");
        dir = path_concat(dir.c_str(), "java-watchdog");
        break;
      case 2:
        dir = "."; // using current working directory
        break;
      case 1:
        dir = progpath(); // trying program-location directory
      default:
        return ""; // tried all possible dir locations but failed to find a config file
    }
    cfg_file_path = path_concat(dir.c_str(), cfg_file_name.data());
    if (stat(cfg_file_path.c_str(), &statbuf) == -1 ||
        ((statbuf.st_mode & S_IFMT) != S_IFREG || (statbuf.st_mode & S_IFMT) == S_IFLNK))
    {
      continue; // try a different dir path location
    }
    break; // found the config file so break out of loop
  }
  return cfg_file_path;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LocalValueEscapesScope"
#pragma ide diagnostic ignored "UnreachableCode"
static void one_time_init_main(int argc, const char *argv[])
{
  s_parent_thrd_pid = getpid();

  const char *malloced_tmpstr = nullptr;

  {
    malloced_tmpstr = strdup(argv[0]); // heap allocates the string storage
    if (malloced_tmpstr == nullptr) {
      __assert("strdup() could not duplicate program full path name on startup", __FILE__, __LINE__);
      _exit(EXIT_FAILURE); // will exit here if __assert() defined to no-op function
    }
    s_progpath = malloced_tmpstr; // static string_view variable takes ownership for program runtime duration
  }

  {
    malloced_tmpstr = [](const char* const path) -> const char* {
      auto const dup_path = strdupa(path); // stack allocates the string storage (will go away on return)
      return strdup(basename(dup_path));   // heap allocates the string storage
    }(progpath());
    if (malloced_tmpstr == nullptr) {
      __assert("strdup() could not duplicate program base name on startup", __FILE__, __LINE__);
      _exit(EXIT_FAILURE); // will exit here if __assert() defined to no-op function
    }
    s_progname = malloced_tmpstr; // static string_view variable takes ownership for program runtime duration
  }

  logger::set_progname(progname());
  logger::set_to_unbuffered();
  log(LL::DEBUG, "starting process %d", get_parent_pid());
}
#pragma clang diagnostic pop

int main(int argc, const char *argv[]) {
  set_level(LL::TRACE); // comment out this line to disable debug/trace logging verbosity
  one_time_init_main(argc, argv);

  // initialized to default settings
  LOGGING_LEVEL logging_level = LL::INFO;
  ACCEPT_ORDINAL accept_ordinal = AO::FIRST_FOUND;

  const auto cfg_file_path = locate_cfg_file();
  if (!cfg_file_path.empty()) {
    auto prs_cfg_callback =
        [&logging_level, &accept_ordinal](const char *section, const char *name, const char *value_cstr)
        {
          const auto to_lower = [](std::string &str) {
            transform(str.begin(), str.end(), str.begin(), ::tolower);
          };

          std::string s_section{section};
          to_lower(s_section);
          if (s_section.compare("settings") == 0) {
            std::string s_name{name};
            to_lower(s_name);
            std::string s_value{value_cstr};
            to_lower(s_value);
            if (s_name.compare("logging_level") == 0) {
              if (s_value.compare("trace") == 0) {
                logging_level = LL::TRACE;
              } else if (s_value.compare("debug") == 0) {
                logging_level = LL::DEBUG;
              } else if (s_value.compare("info") == 0) {
                logging_level = LL::INFO;
              } else if (s_value.compare("warn") == 0) {
                logging_level = LL::WARN;
              } else if (s_value.compare("err") == 0) {
                logging_level = LL::ERR;
              } else {
                logging_level = LL::INFO;
                log(LL::WARN, "logging level '%s' not recognized - defaulting to INFO", value_cstr);
              }
            } else if (s_name.compare("accept_ordinal") == 0) {
              if (s_value.compare("first_found") == 0) {
                accept_ordinal = AO::FIRST_FOUND;
              } else if (s_value.compare("last_found") == 0) {
                accept_ordinal = AO::LAST_FOUND;
              } else if (s_value.compare("second_found") == 0) {
                accept_ordinal = AO::SECOND_FOUND;
              } else if (s_value.compare("third_found") == 0) {
                accept_ordinal = AO::THIRD_FOUND;
              } else if (s_value.compare("fourth_found") == 0) {
                accept_ordinal = AO::FOURTH_FOUND;
              } else if (s_value.compare("fifth_found") == 0) {
                accept_ordinal = AO::FIFTH_FOUND;
              } else if (s_value.compare("sixth_found") == 0) {
                accept_ordinal = AO::SIXTH_FOUND;
              } else if (s_value.compare("seventh_found") == 0) {
                accept_ordinal = AO::SEVENTH_FOUND;
              } else {
                accept_ordinal = AO::FIRST_FOUND;
                log(LL::WARN, "unrecognized settings section %s value '%s' - defaulting to FIRST_FOUND",
                    name, value_cstr);
              }
            } else {
              log(LL::WARN, "unrecognized settings section name '%s' ignored", name);
            }
          } else {
            log(LL::WARN, "unrecognized config section '%s' ignored", section);
          }
          return 1;
        };

    try {
      if (!process_config(cfg_file_path.c_str(), prs_cfg_callback)) {
        // reset to defaults
        logging_level = LL::INFO;
        accept_ordinal = AO::FIRST_FOUND;
      }
    } catch(const process_cfg_exception &ex) {
      // reset to defaults
      logging_level = LL::INFO;
      accept_ordinal = AO::FIRST_FOUND;
      log(LL::WARN, "failed processing config file - using default settings:\n\t%s: %s", ex.name(), ex.what());
    }
  }

  set_level(logging_level);

  // determine the path to the Java launcher program by
  // searching the PATH environment variable path string
  std::string java_prog_path;
  try {
    // determine fully qualified path to the program
    java_prog_path = find_program_path("java", "PATH", accept_ordinal);
  } catch(const find_program_path_exception &ex) {
    log(LL::ERR, "could not locate a Java launcher program:\n\t%s: %s", ex.name(), ex.what());
    return 1;
  }
  log(LL::DEBUG, "Java launcher program: \"%s\"", java_prog_path.c_str());

  return 0;
}