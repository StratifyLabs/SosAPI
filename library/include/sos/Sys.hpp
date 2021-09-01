// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SAPI_SYS_SYS_HPP_
#define SAPI_SYS_SYS_HPP_

#if !defined __link
#include <sos/sos.h>
#endif

#include <sos/dev/sys.h>
#include <sos/link.h>

#include "Appfs.hpp"
#include "Link.hpp"
#include "Trace.hpp"
#include "chrono/DateTime.hpp"
#include "fs/File.hpp"
#include "var/StackString.hpp"

#include "SerialNumber.hpp"

namespace sos {

class Sys : public api::ExecutionContext {
public:
  class Info {
    friend class Sys;

  public:
    Info() = default;
    explicit Info(const sys_info_t &info) : m_info(info) {}

    operator const sys_info_t &() const { return m_info; }

    bool is_valid() const { return cpu_frequency() != 0; }
    var::StringView id() const { return m_info.id; }
    var::StringView team_id() const { return m_info.team_id; }
    var::StringView name() const { return m_info.name; }
    var::StringView system_version() const { return m_info.sys_version; }
    var::StringView bsp_version() const { return m_info.sys_version; }
    var::StringView sos_version() const { return m_info.kernel_version; }
    var::StringView kernel_version() const { return m_info.kernel_version; }
    var::StringView cpu_architecture() const { return m_info.arch; }
    u32 cpu_frequency() const { return m_info.cpu_freq; }
    u32 application_signature() const { return m_info.signature; }
    var::StringView bsp_git_hash() const { return m_info.bsp_git_hash; }
    var::StringView sos_git_hash() const { return m_info.sos_git_hash; }
    var::StringView mcu_git_hash() const { return m_info.mcu_git_hash; }

    u32 o_flags() const { return m_info.o_flags; }

    var::StringView architecture() const { return m_info.arch; }
    var::StringView stdin_name() const { return m_info.stdin_name; }
    var::StringView stdout_name() const { return m_info.stdout_name; }
    var::StringView trace_name() const { return m_info.trace_name; }
    u32 hardware_id() const { return m_info.hardware_id; }

    SerialNumber serial_number() const { return SerialNumber(m_info.serial); }

  private:
    sys_info_t m_info = {};
  };

  Sys() {}
  Sys(const var::StringView device FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);

  Sys(const Sys &a) = delete;
  Sys &operator=(const Sys &a) = delete;

  Sys(Sys &&a) { std::swap(m_file, a.m_file); }
  Sys &operator=(Sys &&a) {
    std::swap(m_file, a.m_file);
    return *this;
  }

#if !defined __link
  var::String get_version();
  var::String get_kernel_version();
#endif

  Info get_info() const;
  bool is_authenticated() const;
  SerialNumber get_serial_number() const;
  sys_id_t get_id() const;

private:
#if defined __link
  Link::File m_file;
#else
  fs::File m_file;
#endif
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Sys::Info &a);
class TraceEvent;
Printer &operator<<(Printer &printer, const sos::TraceEvent &a);
} // namespace printer

#endif /* SAPI_SYS_SYS_HPP_ */
