/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.

#ifndef LINKAPI_LINK_LINK_HPP_
#define LINKAPI_LINK_LINK_HPP_

#if defined __link

#include <mcu/types.h>
#include <sos/link.h>

#include "api/api.hpp"

#include "fs/Dir.hpp"
#include "fs/FileSystem.hpp"
#include "printer/Printer.hpp"
#include "var/String.hpp"
#include "var/Vector.hpp"

#include "sos/Appfs.hpp"
#include "sos/Sys.hpp"

namespace sos {

/*! \brief Link for Controlling Stratify OS remotely
 * \details This class is used to access devices
 * running Stratify OS from a remote platform (desktop/mobile/web).
 *
 * This class is a wrapper for the Stratify OS serialization protocol
 * which allows complete access to Stratify OS system calls
 * and filesystems.
 *
 * This class is not available on native Stratify OS applications.
 *
 *
 */
class Link : public api::ExecutionContext {
public:
  class Info {
  public:
    Info() {}
    Info(const var::StringView path, const sos::Sys::Info &sys_info) {
      set_path(path);
      set_info(sys_info);
    }

    void clear() {
      m_serial_number.clear();
      m_path.clear();
      m_sys_info.clear();
    }

    Info &set_port(const var::StringView port) {
      m_path = port;
      return *this;
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
        m_path =
            var::String(path.get_substring_at_position(host_prefix().length()));
      } else {
        m_driver = driver;
        size_t position;
        if (path.find(device_prefix()) == 0) {
          position = device_prefix().length();
        } else {
          position = 0;
        }
        m_path = var::String(path.get_substring_at_position(position));
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
      return var::PathString(m_driver ? device_prefix() : host_prefix()) +=
             m_path;
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
    var::String m_path;
  };

  /*!
   * \brief The LinkDriverPath class
   * \details The LinkDriverPath class
   * creates parses the details of a link driver path. The path
   * takes one of the following forms
   *
   * ```
   * <serial device path>
   * serial@<serial device path>
   * <driver>@<vendor id>/<product id>/<interface number>
   * <driver>@<vendor id>/<product id>/<interface number>/<serial number>
   * <driver>@<vendor id>/<product id>/<interface number>/<serial
   * number>/<device path>
   * ```
   *
   * - `<driver>` can be `serial` or `usb`
   *
   */
  class DriverPath {
  public:
    DriverPath() {}
    DriverPath(const var::StringView driver_path) {
      set_path(var::String(driver_path));
      var::StringViewList driver_details = m_path.split("@");
      var::StringView details_string;
      if (driver_details.count() == 1) {
        set_driver_name("serial");
        set_path(var::String("serial@") + driver_path);
        details_string = driver_path;
      } else if (driver_details.count() == 2) {
        m_driver_name = var::String(driver_details.at(0));
        details_string = driver_details.at(1);
      }

      if (details_string.is_empty()) {
        return;
      }

      if (details_string.find("/dev") == 0) {
        set_device_path(var::String(details_string));
        return;
      }

      if (details_string.length() && details_string.front() == '/') {
        details_string.pop_front();
      }
      var::StringViewList detail_list = details_string.split("/");

      if (detail_list.count() == 1) {
        set_device_path(var::String(detail_list.at(0)));
      } else if (detail_list.count() > 2) {
        //<driver>@/<vendor id>/<product id>/<interface number>/<serial
        // number>/<device path>
        set_vendor_id(var::String(detail_list.at(0)));
        set_product_id(var::String(detail_list.at(1)));
        set_interface_number(var::String(detail_list.at(2)));
        if (detail_list.count() > 3) {
          set_serial_number(var::String(detail_list.at(3)));
        }
        if (detail_list.count() > 4) {
          set_device_path(var::String(detail_list.at(4)));
        }
      }
    }

    DriverPath &construct_path() {
      set_path(
        driver_name() + "@/" + vendor_id() + "/" + product_id() + "/"
        + interface_number() + "/" + serial_number() + "/" + device_path());

      return *this;
    }

    bool operator==(const DriverPath &a) const {
      // if both values are provided and they are not the same -- they are not
      // the same
      if (
        !vendor_id().is_empty() && !a.vendor_id().is_empty()
        && (vendor_id() != a.vendor_id())) {
        return false;
      }
      if (
        !product_id().is_empty() && !a.product_id().is_empty()
        && (product_id() != a.product_id())) {
        return false;
      }
      if (
        !interface_number().is_empty() && !a.interface_number().is_empty()
        && (interface_number() != a.interface_number())) {
        return false;
      }
      if (
        !serial_number().is_empty() && !a.serial_number().is_empty()
        && (serial_number() != a.serial_number())) {
        return false;
      }
      if (
        !driver_name().is_empty() && !a.driver_name().is_empty()
        && (driver_name() != a.driver_name())) {
        return false;
      }
      if (
        !device_path().is_empty() && !a.device_path().is_empty()
        && (device_path() != a.device_path())) {
        return false;
      }
      return true;
    }

  private:
    API_ACCESS_STRING(DriverPath, path);
    API_ACCESS_STRING(DriverPath, device_path);
    API_ACCESS_STRING(DriverPath, serial_number);
    API_ACCESS_STRING(DriverPath, interface_number);
    API_ACCESS_STRING(DriverPath, vendor_id);
    API_ACCESS_STRING(DriverPath, product_id);
    API_ACCESS_STRING(DriverPath, driver_name);

    var::String lookup_serial_port_path_from_usb_details();
  };

  Link();
  ~Link();

  enum class IsLegacy { no, yes };
  enum class IsBootloader { no, yes };

  fs::PathList get_path_list();

  typedef struct {
    var::String port;
    sos::Sys::Info sys_info;
  } port_device_t;

  var::Vector<Info> get_info_list();

  /*! \details Gets the current progress of an operation.  This allows
   * multi-threaded applications to update a progress bar while copying files.
   */
  int progress() const { return m_progress; }

  /*! \details Gets the maximum progress value of the current operation.
   *
   */
  int progress_max() const { return m_progress_max; }

  Link &connect(var::StringView path, IsLegacy is_legacy = IsLegacy::no);
  Link &reinitialize() { return connect(path()); }
  Link &reconnect(int retries = 5, chrono::MicroTime delay = 500_milliseconds);
  Link &disconnect();
  Link &set_disconnected();

  bool ping(const var::StringView path);

  /*! \details Returns true if the device is connected.
   */
  bool is_connected() const;
  bool get_is_connected() const { return is_connected(); }

  // These are all the file transfer options over Stratify OS Link

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

  bool is_legacy() const { return m_is_legacy == IsLegacy::yes; }

  Link &write_flash(int addr, const void *buf, int nbyte);
  Link &read_flash(int addr, void *buf, int nbyte);

  Link &get_time(struct tm *gt);
  Link &set_time(struct tm *gt);

  class UpdateOs {
    API_AF(UpdateOs, const fs::File *, image, nullptr);
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

  Link &set_progress(int p) {
    m_progress = p;
    return *this;
  }
  Link &set_progress_max(int p) {
    m_progress_max = p;
    return *this;
  }

  var::StringView serial_number() const {
    return m_link_info.serial_number().string_view();
  }

  var::StringView path() const { return m_link_info.port(); }

  const sos::Sys::Info &sys_info() const { return m_link_info.sys_info(); }

  const Info &info() const { return m_link_info; }

private:
  var::String m_notify_path;
  var::String m_error_message;
  int m_stdout_fd = -1;
  int m_stdin_fd = -1;
  volatile int m_progress = 0;
  volatile int m_progress_max = 0;
  volatile int m_lock = 0;
  IsBootloader m_is_bootloader = IsBootloader::no;
  IsLegacy m_is_legacy = IsLegacy::no;

  Info m_link_info;
  bootloader_attr_t m_bootloader_attributes = {0};

  link_transport_mdriver_t m_driver_instance = {0};

  u32 validate_os_image_id_with_connected_bootloader(
      const fs::File *source_image);

  Link &erase_os(const UpdateOs &options);

  Link &install_os(u32 image_id, const UpdateOs &options);

  Link &reset_progress();
};

} // namespace sos

#endif // link

#endif // LINKAPI_LINK_LINK_HPP_
