/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.

#ifndef LINKAPI_LINK_LINK_HPP_
#define LINKAPI_LINK_LINK_HPP_

#if defined __link

#include <mcu/types.h>
#include <sos/link.h>

#include <api/api.hpp>

#include <fs/Dir.hpp>
#include <fs/FileSystem.hpp>
#include <printer/Printer.hpp>
#include <var/String.hpp>
#include <var/Tokenizer.hpp>
#include <var/Vector.hpp>

#include "sos/Appfs.hpp"
#include "sos/Sys.hpp"

namespace sos {

class Link : public api::ExecutionContext {
public:
  enum class Type { serial, usb };
  enum class IsLegacy { no, yes };
  enum class IsBootloader { no, yes };

  class Info {
  public:
    Info() {}
    Info(const var::StringView path, const sos::Sys::Info &sys_info) {
      set_path(path);
      set_info(sys_info);
    }

    Info &set_info(const sos::Sys::Info &sys_info) {
      m_sys_info = sys_info;
      m_serial_number = sys_info.serial_number().to_string();
      return *this;
    }

    const var::StringView port() const { return m_path.string_view(); }

  private:
    API_ACCESS_COMPOUND(Info, var::PathString, path);
    API_READ_ACCESS_COMPOUND(Info, sos::Sys::Info, sys_info);
    API_READ_ACCESS_COMPOUND(Info, var::KeyString, serial_number);
  };

  class Path {
  public:
    Path() { m_driver = nullptr; }

    Path(const var::StringView path, link_transport_mdriver_t *driver) {

      if (path.find(host_prefix()) == 0) {
        m_driver = nullptr;
        m_path = path.get_substring_at_position(host_prefix().length());
      } else {
        m_driver = driver;
        size_t position;
        if (path.find(device_prefix()) == 0) {
          position = device_prefix().length();
        } else {
          position = 0;
        }
        m_path = path.get_substring_at_position(position);
      }
    }

    bool is_valid() const { return !m_path.is_empty(); }

    static bool is_device_path(const var::StringView path) {
      return path.find(device_prefix()) == 0;
    }

    static bool is_host_path(const var::StringView path) {
      return path.find(host_prefix()) == 0;
    }

    static var::StringView device_prefix() { return "device@"; }

    static var::StringView host_prefix() { return "host@"; }

    var::PathString path_description() const {
      return var::PathString(m_driver ? device_prefix() : host_prefix())
             += m_path;
    }

    bool is_device_path() const { return m_driver != nullptr; }

    bool is_host_path() const { return m_driver == nullptr; }

    var::StringView prefix() const {
      return is_host_path() ? host_prefix() : device_prefix();
    }

    const var::StringView path() const { return m_path.string_view(); }

    link_transport_mdriver_t *driver() const { return m_driver; }

  private:
    link_transport_mdriver_t *m_driver;
    var::PathString m_path;
  };

  /*
   * /
   * <driver>/
   * <vendor id>/
   * <product id>/
   * <interface number>/
   * <serial number>/
   * <device path>
   * ```
   *
   * - `<driver>` can be `serial` or `usb`
   *
   */
  class DriverPath {
  public:
    class Construct {
      API_AF(Construct, Type, type, Type::usb);
      API_AC(Construct, var::StringView, vendor_id);
      API_AC(Construct, var::StringView, product_id);
      API_AC(Construct, var::StringView, interface_number);
      API_AC(Construct, var::StringView, serial_number);
      API_AC(Construct, var::StringView, device_path);
    };

    DriverPath(const Construct &options) {
      set_path(
        var::PathString() / (options.type() == Type::serial ? "serial" : "usb")
        / options.vendor_id() / options.product_id()
        / options.interface_number() / options.serial_number()
        / options.device_path());
    }

    DriverPath(const var::StringView driver_path) {
      API_ASSERT(is_valid(driver_path));
      set_path(driver_path);
    }

    static bool is_valid(const var::StringView driver) {
      const auto list = driver.split("/");
      if (list.count() < 2) {
        return false;
      }

      if (list.at(0).is_empty() == false) {
        return false;
      }

      if (
        list.at(get_position(Position::driver_name)) != "usb"
        && list.at(get_position(Position::driver_name)) != "serial") {
        return false;
      }

      return true;
    }

    Type get_type() const {
      if (get_driver_name() == "usb") {
        return Type::usb;
      }
      return Type::serial;
    }

    var::StringView get_driver_name() const {
      return get_value_at_position(Position::driver_name);
    }

    var::StringView get_vendor_id() const {
      return get_value_at_position(Position::vendor_id);
    }

    var::StringView get_product_id() const {
      return get_value_at_position(Position::product_id);
    }

    var::StringView get_interface_number() const {
      return get_value_at_position(Position::interface_number);
    }

    var::StringView get_serial_number() const {
      return get_value_at_position(Position::serial_number);
    }

    var::StringView get_device_path() const {
      return get_value_at_position(Position::device_path);
    }

    bool operator==(const DriverPath &a) const {
      // if both values are provided and they are not the same -- they are not
      // the same
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
        !get_interface_number().is_empty()
        && !a.get_interface_number().is_empty()
        && (get_interface_number() != a.get_interface_number())) {
        return false;
      }
      if (
        !get_serial_number().is_empty() && !a.get_serial_number().is_empty()
        && (get_serial_number() != a.get_serial_number())) {
        return false;
      }
      if (
        !get_driver_name().is_empty() && !a.get_driver_name().is_empty()
        && (get_driver_name() != a.get_driver_name())) {
        return false;
      }
      if (
        !get_device_path().is_empty() && !a.get_device_path().is_empty()
        && (get_device_path() != a.get_device_path())) {
        return false;
      }
      return true;
    }

  private:
    API_AC(DriverPath, var::PathString, path);

    enum class Position {
      null,
      driver_name,
      vendor_id,
      product_id,
      interface_number,
      serial_number,
      device_path,
      last_position = device_path
    };

    var::StringView get_value_at_position(Position position) const {
      const auto list = split();
      if (list.count() > get_position(position)) {
        return list.at(get_position(position));
      }
      return var::StringView();
    }

    var::Tokenizer split() const {
      return var::Tokenizer(
        path(),
        var::Tokenizer::Construct()
          .set_delimeters("/")
          .set_maximum_delimeter_count(6));
    }

    static size_t get_position(Position value) {
      return static_cast<size_t>(value);
    }

    static size_t position_count() {
      return static_cast<size_t>(Position::last_position) + 1;
    }

    var::String lookup_serial_port_path_from_usb_details();
  };

  Link();
  ~Link();

  fs::PathList get_path_list();

  using InfoList = var::Vector<Info>;
  InfoList get_info_list();

  Link &connect(var::StringView path, IsLegacy is_legacy = IsLegacy::no);
  bool is_legacy() const { return m_is_legacy == IsLegacy::yes; }
  Link &reconnect(int retries = 5, chrono::MicroTime delay = 500_milliseconds);
  Link &disconnect();
  Link &disregard_connection();

  bool ping(const var::StringView path);
  bool is_connected() const;

  static var::KeyString convert_permissions(link_mode_t mode);

  Link &format(const var::StringView path); // Format the drive
  Link &run_app(const var::StringView path);

  Link &reset();
  Link &reset_bootloader();
  Link &get_bootloader_attr(bootloader_attr_t &attr);
  bool is_bootloader() const { return m_is_bootloader == IsBootloader::yes; }
  bool is_connected_and_is_not_bootloader() const {
    return is_connected() && !is_bootloader();
  }


  Link &write_flash(int addr, const void *buf, int nbyte);
  Link &read_flash(int addr, void *buf, int nbyte);

  Link &get_time(struct tm *gt);
  Link &set_time(struct tm *gt);

  class UpdateOs {
    API_AF(UpdateOs, const fs::FileObject *, image, nullptr);
    API_AF(UpdateOs, u32, bootloader_retry_count, 20);
    API_AF(UpdateOs, printer::Printer *, printer, nullptr);
    API_AB(UpdateOs, verify, false);
  };

  Link &update_os(const UpdateOs &options);
  inline Link &operator()(const UpdateOs &options) {
    return update_os(options);
  }

  const link_transport_mdriver_t *driver() const { return &m_driver_instance; }
  link_transport_mdriver_t *driver() { return &m_driver_instance; }

  Link &set_driver_options(const void *options) {
    m_driver_instance.options = options;
    return *this;
  }

  Link &set_driver(link_transport_mdriver_t *driver) {
    m_driver_instance = *driver;
    return *this;
  }

  int progress() const { return m_progress; }
  int progress_max() const { return m_progress_max; }

  Link &set_progress(int p) {
    m_progress = p;
    return *this;
  }

  Link &set_progress_max(int p) {
    m_progress_max = p;
    return *this;
  }

  const Info &info() const { return m_link_info; }

private:
  volatile int m_progress = 0;
  volatile int m_progress_max = 0;
  IsBootloader m_is_bootloader = IsBootloader::no;
  IsLegacy m_is_legacy = IsLegacy::no;

  Info m_link_info;

  bootloader_attr_t m_bootloader_attributes = {0};
  link_transport_mdriver_t m_driver_instance = {0};

  u32 validate_os_image_id_with_connected_bootloader(
    const fs::FileObject *source_image);

  Link &erase_os(const UpdateOs &options);
  Link &install_os(u32 image_id, const UpdateOs &options);
  Link &reset_progress();

  enum class Connection { null, bootloader, os };

  Connection ping_connection(const var::StringView path);
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Link::Info &a);
Printer &operator<<(Printer &printer, const sos::Link::InfoList &a);
} // namespace printer

#endif // link

#endif // LINKAPI_LINK_LINK_HPP_
