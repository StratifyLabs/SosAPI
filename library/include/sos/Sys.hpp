/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.

#ifndef SAPI_SYS_SYS_HPP_
#define SAPI_SYS_SYS_HPP_

#if !defined __link
#include <sos/sos.h>
#endif

#include <sos/dev/sys.h>
#include <sos/link.h>

#include "Appfs.hpp"
#include "Trace.hpp"
#include "chrono/DateTime.hpp"
#include "fs/File.hpp"
#include "var/StackString.hpp"

namespace sos {

class Sys;

/*! \brief Serial Number class
 * \details The SerialNumber class holds a value for an
 * MCU serial number.
 *
 * Stratify OS supports reading the serial number directly
 * from the chip. This class makes doing so as simply as possible.
 *
 */
class SerialNumber {
public:
  /*! \details Constructs an empty serial number. */
  SerialNumber();

  /*! \details Constructs a serial number for an array of u32 values. */
  explicit SerialNumber(const u32 serial_number[4]) {
    memcpy(m_serial_number.sn, serial_number, sizeof(u32) * 4);
  }

  /*! \details Constructs a serial number from an mcu_sn_t. */
  explicit SerialNumber(const mcu_sn_t serial_number)
    : m_serial_number(serial_number) {}

  /*! \details Constructs this serial number from \a str. */
  explicit SerialNumber(var::StringView str);

  /*! \details Returns true if a valid serial number is held. */
  bool is_valid() const { return at(0) + at(1) + at(2) + at(3) != 0; }

  /*! \details Returns the u32 section of the serial number specified by *idx*.
   */
  u32 at(u32 idx) const {
    if (idx >= 4) {
      idx = 3;
    }
    return m_serial_number.sn[idx];
  }

  /*! \details Compares this strig to \a serial_number. */
  bool operator==(const SerialNumber &serial_number);

  /*! \details Converts the serial number to a string. */
  var::KeyString to_string() const;

  /*! \details Returns a serial number object from a string type. */
  static SerialNumber from_string(var::StringView str);

private:
  mcu_sn_t m_serial_number;
};

class Sys : public api::ExecutionContext {
public:
  class Info {
    friend class Sys;

  public:
    Info() { m_info = {0}; }

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
    sys_info_t m_info;
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
  int get_board_config(sos_board_config_t &config);
#endif

  Info get_info() const;
  bool is_authenticated() const;
  sys_secret_key_t get_secret_key() const;
  SerialNumber get_serial_number() const;
  sys_id_t get_id() const;

private:
  fs::File m_file;
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Sys::Info &a);
class TraceEvent;
Printer &operator<<(Printer &printer, const sos::TraceEvent &a);
} // namespace printer

#endif /* SAPI_SYS_SYS_HPP_ */
