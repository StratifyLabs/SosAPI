//
// Created by Tyler Gilbert on 3/18/22.
//

#include "sos/Link.hpp"

using namespace sos;

Link::DriverPath::DriverPath(const Link::DriverPath::Construct &options) {
  if (options.type() == Type::serial) {
    set_path(var::PathString("@serial") / options.device_path());
  } else {
    set_path(
      var::PathString("@usb") / options.vendor_id() / options.product_id()
      / options.interface_number() / options.serial_number()
      / options.device_path());
  }
}

Link::DriverPath::DriverPath(const var::StringView driver_path) {
  set_path(driver_path);
}

bool Link::DriverPath::is_valid() const {

  if (path().is_empty()) {
    return true;
  }

  if (get_type() == Type::null) {
    return false;
  }

  return true;
}

Link::Type Link::DriverPath::get_type() const {
  if (get_driver_name() == "usb") {
    return Type::usb;
  }
  if (get_driver_name() == "serial") {
    return Type::serial;
  }
  return Type::null;
}

var::StringView Link::DriverPath::get_driver_name() const {
  return get_value_at_position(Position::driver_name);
}

var::StringView Link::DriverPath::get_vendor_id() const {
  if (get_type() == Type::usb) {
    return get_value_at_position(Position::vendor_id_or_device_path);
  }
  return var::StringView();
}

var::StringView Link::DriverPath::get_product_id() const {
  if (get_type() == Type::usb) {
    return get_value_at_position(Position::product_id);
  }
  return var::StringView();
}

var::StringView Link::DriverPath::get_interface_number() const {
  if (get_type() == Type::usb) {
    return get_value_at_position(Position::interface_number);
  }
  return var::StringView();
}

var::StringView Link::DriverPath::get_serial_number() const {
  if (get_type() == Type::usb) {
    return get_value_at_position(Position::serial_number);
  }
  return var::StringView();
}

var::StringView Link::DriverPath::get_device_path() const {
  if (get_type() == Type::serial) {
    return get_value_at_position(Position::vendor_id_or_device_path);
  }
  return var::StringView();
}

bool Link::DriverPath::is_partial() const {
  if (get_type() == Type::usb) {
    if (get_vendor_id().is_empty()) {
      return true;
    }

    if (get_product_id().is_empty()) {
      return true;
    }

    if (get_serial_number().is_empty()) {
      return true;
    }

    if (get_interface_number().is_empty()) {
      return true;
    }

    return false;
  }

  if (get_type() == Type::serial) {
    return get_device_path().is_empty();
  }

  return true;
}

bool Link::DriverPath::operator==(const Link::DriverPath &a) const {
  // if both values are provided and they are not the same -- they are not

  if (get_type() == Type::serial && a.get_type() == Type::serial) {
    if (
      !get_device_path().is_empty() && !a.get_device_path().is_empty()
      && get_device_path() != a.get_device_path()) {
      return false;
    }
    return true;
  }

  if (
    (get_type() != Type::null) && (a.get_type() != Type::null)
    && (get_type() != a.get_type())) {
    return false;
  }

  if (
    !get_vendor_id().is_empty() && !a.get_vendor_id().is_empty()
    && (get_vendor_id() != a.get_vendor_id())) {
    return false;
  }
  if (
    !get_product_id().is_empty() && !a.get_product_id().is_empty()
    && (get_product_id() != a.get_product_id())) {
    return false;
  }
  if (
    !get_interface_number().is_empty() && !a.get_interface_number().is_empty()
    && (get_interface_number() != a.get_interface_number())) {
    return false;
  }
  if (
    !get_serial_number().is_empty() && !a.get_serial_number().is_empty()
    && (get_serial_number() != a.get_serial_number())) {
    return false;
  }

  if (
    !get_device_path().is_empty() && !a.get_device_path().is_empty()
    && (get_device_path() != a.get_device_path())) {
    return false;
  }
  return true;
}

var::StringView Link::DriverPath::get_value_at_position(
  Link::DriverPath::Position position) const {
  const auto list = split();
  if (list.count() > get_position(position)) {
    return list.at(get_position(position));
  }
  return var::StringView();
}

var::Tokenizer Link::DriverPath::split() const {
  return var::Tokenizer(
    path(),
    var::Tokenizer::Construct()
      .set_delimeters("/@")
      .set_maximum_delimeter_count(6));
}

var::String Link::DriverPath::lookup_serial_port_path_from_usb_details() {

#if defined __win32

#endif
  return var::String();
}