#ifndef SYSAPI_SYS_SOS_HPP
#define SYSAPI_SYS_SOS_HPP

#if !defined __link

#include <api/api.hpp>

#include <chrono/MicroTime.hpp>
#include <var/String.hpp>
#include <var/StringView.hpp>

#include "Appfs.hpp"

namespace sos {

class Sos : public api::ExecutionContext {
public:
  void reset();

  class Launch {
    API_ACCESS_COMPOUND(Launch, var::StringView, path);
    API_ACCESS_COMPOUND(Launch, var::StringView, arguments);
    API_ACCESS_COMPOUND(Launch, var::StringView, environment);
    API_ACCESS_FUNDAMENTAL(
      Launch,
      Appfs::Flags,
      application_flags,
      Appfs::Flags::is_default);
    API_ACCESS_FUNDAMENTAL(Launch, int, ram_size, 0);
    API_ACCESS_FUNDAMENTAL(
      Launch,
      const api::ProgressCallback *,
      progress_callback,
      nullptr);
  };

  var::String launch(const Launch &options) const;

  var::String install(
    var::StringView path,
    Appfs::Flags options
    = Appfs::Flags::is_default, // run in RAM, discard on exit
    int ram_size = 0) const;

  var::String install(
    var::StringView path,
    Appfs::Flags options, // run in RAM, discard on exit
    int ram_size,
    const api::ProgressCallback *progress_callback) const;

  void powerdown(const chrono::MicroTime &duration = 0_milliseconds);

  const Sos &
  hibernate(const chrono::MicroTime &duration = 0_milliseconds) const;

  int request(int req, void *argument = nullptr) const;

  static void redirect_stdout(int fd) { _impure_ptr->_stdout->_file = fd; }
  static void redirect_stdin(int fd) { _impure_ptr->_stdin->_file = fd; }

  static void redirect_stderr(int fd) { _impure_ptr->_stderr->_file = fd; }
};

} // namespace sys

#endif
#endif // SYSAPI_SYS_SOS_HPP
