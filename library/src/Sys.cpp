// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#if !defined __link
#include <sos/sos.h>
#endif

#include <sos/link.h>

#include "printer/Printer.hpp"
#include "sos/Sys.hpp"
#include "sos/Trace.hpp"

printer::Printer &
printer::operator<<(printer::Printer &printer, const sos::TraceEvent &a) {
  var::String id;
  chrono::ClockTime clock_time;
  clock_time = a.timestamp();
  switch (a.id()) {
  case LINK_POSIX_TRACE_FATAL:
    id = "fatal";
    break;
  case LINK_POSIX_TRACE_CRITICAL:
    id = "critical";
    break;
  case LINK_POSIX_TRACE_WARNING:
    id = "warning";
    break;
  case LINK_POSIX_TRACE_MESSAGE:
    id = "message";
    break;
  case LINK_POSIX_TRACE_ERROR:
    id = "error";
    break;
  default:
    id = "other";
    break;
  }
  printer.key(
    "timestamp",
    var::String().format(
      F32U ".%06ld",
      clock_time.seconds(),
      clock_time.nanoseconds() / 1000UL));
  printer.key("id", id);
  printer.key("thread", var::NumberString(a.thread_id()).string_view());
  printer.key("pid", var::NumberString(a.pid()).string_view());
  printer.key(
    "programAddress",
    var::NumberString(a.program_address(), "0x%lX").string_view());
  printer.key("message", a.message());
  return printer;
}

printer::Printer &
printer::operator<<(printer::Printer &printer, const sos::Sys::Info &a) {
  printer.key("name", a.name());
  printer.key("serialNumber", a.serial_number().to_string());
  printer.key(
    "hardwareId",
    var::NumberString(a.hardware_id(), F3208X).string_view());
  if (a.name() != "bootloader") {
    printer.key("projectId", a.id());
    if (a.team_id().is_empty() == false) {
      printer.key("team", a.team_id());
    } else {
      printer.key("team", "<none>");
    }
    printer.key("bspVersion", a.bsp_version());
    printer.key("sosVersion", a.sos_version());
    printer.key("cpuArchitecture", a.cpu_architecture());
    printer.key(
      "cpuFrequency",
      var::NumberString(a.cpu_frequency()).string_view());

    printer.key(
      "applicationSignature",
      var::NumberString(a.application_signature(), F32X).string_view());

    printer.key("bspGitHash", a.bsp_git_hash());
    printer.key("sosGitHash", a.sos_git_hash());
    printer.key("mcuGitHash", a.mcu_git_hash());
  }
  return printer;
}

using namespace sos;

Sys::Sys(const var::StringView device_path FSAPI_LINK_DECLARE_DRIVER_LAST)
  : m_file(
    device_path.is_empty() ? "/dev/sys" : device_path,
    fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {}

Sys::Info Sys::get_info() const {
  sys_info_t sys_info = {0};
  m_file.ioctl(I_SYS_GETINFO, &sys_info);
  return Sys::Info(sys_info);
}

bool Sys::is_authenticated() const {
  return m_file.ioctl(I_SYS_ISAUTHENTICATED, nullptr).return_value();
}

sys_secret_key_t Sys::get_secret_key() const {
  sys_secret_key_t result = {0};
  m_file.ioctl(I_SYS_GETSECRETKEY, &result);
  return result;
}

SerialNumber Sys::get_serial_number() const {
  return get_info().serial_number();
}

sys_id_t Sys::get_id() const {
  sys_id_t result = {0};
  m_file.ioctl(I_SYS_GETID, &result);
  return result;
}
