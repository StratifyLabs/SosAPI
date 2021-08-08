// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOSAPI_SOS_SOS_HPP
#define SOSAPI_SOS_SOS_HPP

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

  var::PathString launch(const Launch &options) const;

  var::PathString install(
    var::StringView path,
    Appfs::Flags options
    = Appfs::Flags::is_default, // run in RAM, discard on exit
    int ram_size = 0) const;

  var::PathString install(
    var::StringView path,
    Appfs::Flags options, // run in RAM, discard on exit
    int ram_size,
    const api::ProgressCallback *progress_callback) const;

  enum class NoHang {
    no, yes
  };

  Sos & wait_pid(NoHang no_hang = NoHang::no, int pid = -1);

  void powerdown(const chrono::MicroTime &duration = 0_milliseconds);

  const Sos &
  hibernate(const chrono::MicroTime &duration = 0_milliseconds) const;

  int request(int req, void *argument = nullptr) const;

  static void redirect_stdout(int fd) { _impure_ptr->_stdout->_file = fd; }
  static void redirect_stdin(int fd) { _impure_ptr->_stdin->_file = fd; }

  static void redirect_stderr(int fd) { _impure_ptr->_stderr->_file = fd; }

private:
  API_RAF(Sos, int, child_status, 0);
};

} // namespace sys

#endif
#endif // SOSAPI_SOS_SOS_HPP
