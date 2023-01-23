/* cfgparse.cpp

Copyright 2015 - 2016 Tideworks Technology
Author: Roger D. Voss
 Modifications made Jan. 2023 by R.D. Voss

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
#include <sys/stat.h>
#include <list>
#include <sstream>
#include "format2str.h"
#include "cfgparse.h"

static const char config_file_parse_err_fmt[] = "config file parsing error %d in %s() at line %d\n";
static const char config_file_load_err_fmt[]  = "can't load config file \"%s\"\n";

bool process_config(const char * const cfg_full_filepath, const cfg_parse_handler_t &handler) {
  // check to see if specified config file exist
  struct stat statbuf{};
  if (stat(cfg_full_filepath, &statbuf) == -1 ||
      ((statbuf.st_mode & S_IFMT) != S_IFREG || (statbuf.st_mode & S_IFMT) == S_IFLNK))
  {
    return false;
  }

  std::list<std::string> err_list;

  auto const err_code_notify = [&err_list](int ec, const char* op, int ln) {
    auto errmsg( format2str(config_file_parse_err_fmt, ec, op, ln) );
    err_list.emplace_back(std::move(errmsg));
  };

  if (ini_parse(cfg_full_filepath, handler, err_code_notify) != 0) {
    auto errmsg( format2str(config_file_load_err_fmt, cfg_full_filepath) );
    err_list.emplace_front(std::move(errmsg));

    std::stringstream ss;
    for(std::list<std::string>::iterator list_iter = err_list.begin(); list_iter != err_list.end(); list_iter++)
    {
      ss << *list_iter;
    }
    err_list.clear();
    throw process_cfg_exception(ss.str());
  }

  return true;
}
