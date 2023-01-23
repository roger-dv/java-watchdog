/* decl-exception.h

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
#ifndef __DECL_EXCEPTION_H__
#define __DECL_EXCEPTION_H__

#include <exception>
#include <memory>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
class watchdog_exception : public std::exception {
protected:
  virtual void make_abstract() = 0;
protected:
  static void free_nm(char *p);
  std::unique_ptr<char, decltype(&free_nm)> name_fld{nullptr, &free_nm };
  std::string msg_fld;
  char* type_name(const char * const mangled_name);
  explicit watchdog_exception(const char * const mangled_type_name, const char * const msg)
    : msg_fld{msg } { name_fld.reset(type_name(mangled_type_name)); }
  explicit watchdog_exception(const char * const mangled_type_name, std::string &msg)
    : msg_fld{std::move(msg) } { name_fld.reset(type_name(mangled_type_name)); }
  watchdog_exception() = default;
public:
  watchdog_exception(const char * const msg) = delete;
  watchdog_exception(std::string &&) = delete;
  watchdog_exception(const std::string &) = delete;
  watchdog_exception(std::string &) = delete;
  watchdog_exception(const watchdog_exception &) = delete;
  watchdog_exception& operator=(const watchdog_exception &) = delete;
  watchdog_exception(watchdog_exception &&) = delete;
  watchdog_exception& operator=(watchdog_exception &&) = delete;
  ~watchdog_exception() override = default;
public:
  virtual const char* name() const throw()  { return name_fld.get(); }
  const char* what() const throw() override { return msg_fld.c_str(); }
};
#pragma GCC diagnostic pop

#define DECL_EXCEPTION(x) \
class x##_exception : public watchdog_exception {\
protected:\
  void make_abstract() override {}\
public:\
  x##_exception() = delete;\
  explicit x##_exception(const char * const msg) : watchdog_exception{ typeid(*this).name(), msg } {}\
  explicit x##_exception(std::string &&msg) : watchdog_exception{ typeid(*this).name(), msg } {}\
  x##_exception(const std::string &) = delete;\
  x##_exception(std::string &) = delete;\
  x##_exception(const x##_exception &) = delete;\
  x##_exception& operator=(const x##_exception &) = delete;\
  x##_exception(x##_exception &&ex) noexcept : watchdog_exception() { this->operator=(std::move(ex)); }\
  x##_exception& operator=(x##_exception &&ex) noexcept {\
    this->name_fld  = std::move(ex.name_fld);\
    this->msg_fld = std::move(ex.msg_fld);\
    return *this;\
  }\
  ~x##_exception() override = default;\
};

std::string get_unmangled_name(const char * const mangled_name);

#endif // __DECL_EXCEPTION_H__