#include <var/View.hpp>

#include "sos/SerialNumber.hpp"

using namespace sos;

SerialNumber::SerialNumber() { m_serial_number = {0}; }

SerialNumber SerialNumber::from_string(var::StringView str) {
  SerialNumber ret;
  if (str.length() == 8 * 4) {
    const var::StackString64 s(str);
#if defined __link
    sscanf(
      s.cstring(),
      "%08X%08X%08X%08X",
#else
    sscanf(
      s.cstring(),
      "%08lX%08lX%08lX%08lX",
#endif
      &ret.m_serial_number.sn[3],
      &ret.m_serial_number.sn[2],
      &ret.m_serial_number.sn[1],
      &ret.m_serial_number.sn[0]);
  }
  return ret;
}

SerialNumber::SerialNumber(var::StringView str) {
  SerialNumber serial_number = from_string(str);
  var::View(m_serial_number).copy(var::View(serial_number.m_serial_number));
}

bool SerialNumber::operator==(const SerialNumber &serial_number) {
  return memcmp(
           &serial_number.m_serial_number,
           &m_serial_number,
           sizeof(mcu_sn_t))
         == 0;
}

var::KeyString SerialNumber::to_string() const {
  return var::KeyString().format(
    F3208X F3208X F3208X F3208X,
    m_serial_number.sn[3],
    m_serial_number.sn[2],
    m_serial_number.sn[1],
    m_serial_number.sn[0]);
}
