// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOSAPI_SOS_SERIALNUMBER_HPP
#define SOSAPI_SOS_SERIALNUMBER_HPP

#include <var/StackString.hpp>

namespace sos {

class SerialNumber {
public:
  SerialNumber();

  explicit SerialNumber(const u32 serial_number[4]) {
    memcpy(m_serial_number.sn, serial_number, sizeof(u32) * 4);
  }

  explicit SerialNumber(const mcu_sn_t serial_number)
    : m_serial_number(serial_number) {}

  explicit SerialNumber(var::StringView str);
  bool is_valid() const { return at(0) + at(1) + at(2) + at(3) != 0; }
  u32 at(u32 idx) const {
    if (idx >= 4) {
      idx = 3;
    }
    return m_serial_number.sn[idx];
  }

  bool operator==(const SerialNumber &serial_number) const;
  var::KeyString to_string() const;
  static SerialNumber from_string(var::StringView str);

private:
  mcu_sn_t m_serial_number;
};

} // namespace sos

#endif // SOSAPI_SOS_SERIALNUMBER_HPP
