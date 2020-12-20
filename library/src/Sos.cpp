// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#if !defined __link
#include <sos/sos.h>

#include <var/StackString.hpp>

#include "sos/Sos.hpp"

using namespace sos;

void Sos::powerdown(const chrono::MicroTime &duration) {
  API_RETURN_IF_ERROR();
  ::powerdown(duration.seconds());
}

const Sos &Sos::hibernate(const chrono::MicroTime &duration) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  API_SYSTEM_CALL("", ::hibernate(duration.seconds()));
  return *this;
}

int Sos::request(int request, void *arg) const {
  API_RETURN_VALUE_IF_ERROR(-1);
  return kernel_request(request, arg);
}

void Sos::reset() {
  API_RETURN_IF_ERROR();
  int fd = ::open("/dev/core", O_RDWR);
  core_attr_t attr;
  attr.o_flags = CORE_FLAG_EXEC_RESET;
  ::ioctl(fd, I_CORE_SETATTR, &attr);
  ::close(fd); // incase reset fails
}

var::String Sos::launch(const Launch &options) const {
  API_RETURN_VALUE_IF_ERROR(var::String());
  var::String result;
  const var::PathString path_string(options.path());
  const var::GeneralString argument_string(options.arguments());
  result.resize(PATH_MAX);
  if (
    API_SYSTEM_CALL(
      "",
      ::launch(
        path_string.cstring(),
        result.to_char(),
        argument_string.cstring(),
        static_cast<int>(options.application_flags()),
        options.ram_size(),
        api::ProgressCallback::update_function,
        (void *)(options.progress_callback()), // pointer to the object
        nullptr                                // environment not implemented
        ))
    < 0) {
    return var::String();
  }
  return result;
}

var::String Sos::install(
  var::StringView path,
  Appfs::Flags options, // run in RAM, discard on exit
  int ram_size) const {
  API_RETURN_VALUE_IF_ERROR(var::String());
  return install(path, options, ram_size, nullptr);
}

var::String Sos::install(
  var::StringView path,
  Appfs::Flags options, // run in RAM, discard on exit
  int ram_size,
  const api::ProgressCallback *progress_callback) const {
  const var::PathString path_string(path);
  var::String result;
  result.resize(PATH_MAX);
  API_RETURN_VALUE_IF_ERROR(var::String());
  if (
    API_SYSTEM_CALL(
      "",
      ::install(
        path_string.cstring(),
        result.to_char(),
        static_cast<int>(options),
        ram_size,
        api::ProgressCallback::update_function,
        progress_callback))
    < 0) {
    return var::String();
  }
  return result;
}

#else
int sys_api_sos_unused = 0;
#endif
