/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.

#ifndef LINKAPI_LINK_LINK_HPP_
#define LINKAPI_LINK_LINK_HPP_

#include "macros.hpp"

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

#include "SerialNumber.hpp"

namespace sos {

class Link : public api::ExecutionContext {
public:
  enum class Type { null, serial, usb };
  enum class IsLegacy { no, yes };
  enum class IsBootloader { no, yes };
  enum class IsKeepConnection { no, yes };

  class Info {
  public:
    Info() { m_info = {0}; }
    Info(const var::StringView path, const sys_info_t &sys_info) {
      set_path(path);
      m_info = sys_info;
    }

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

    const sys_info_t &sys_info() const { return m_info; }

  private:
    API_ACCESS_COMPOUND(Info, var::PathString, path);
    sys_info_t m_info;
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
             & m_path;
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
   * usb/
   * <vendor id>/
   * <product id>/
   * <interface number>/
   * <serial number>
   *
   * /
   * serial/
   * device_path
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

    DriverPath() {}

    DriverPath(const Construct &options) {
      set_path(
        var::PathString() / (options.type() == Type::serial ? "serial" : "usb")
        / options.vendor_id() / options.product_id()
        / options.interface_number() / options.serial_number()
        / options.device_path());
    }

    DriverPath(const var::StringView driver_path) {
      set_path(driver_path);
    }

    bool is_valid() const {

      if (path().is_empty()) {
        return true;
      }

      if (get_type() == Type::null) {
        return false;
      }

      return true;
    }

    Type get_type() const {
      if (get_driver_name() == "usb") {
        return Type::usb;
      }
      if (get_driver_name() == "serial") {
        return Type::serial;
      }
      return Type::null;
    }

    var::StringView get_driver_name() const {
      return get_value_at_position(Position::driver_name);
    }

    var::StringView get_vendor_id() const {
      if (get_type() == Type::usb) {
        return get_value_at_position(Position::vendor_id);
      }
      return var::StringView();
    }

    var::StringView get_product_id() const {
      if (get_type() == Type::usb) {
        return get_value_at_position(Position::product_id);
      }
      return var::StringView();
    }

    var::StringView get_interface_number() const {
      if (get_type() == Type::usb) {
        return get_value_at_position(Position::interface_number);
      }
      return var::StringView();
    }

    var::StringView get_serial_number() const {
      if (get_type() == Type::usb) {
        return get_value_at_position(Position::serial_number);
      }
      return var::StringView();
    }

    var::StringView get_device_path() const {
      if (get_type() == Type::serial) {
        return get_value_at_position(Position::device_path);
      }
      return var::StringView();
    }

    bool is_partial() const {
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

    bool operator==(const DriverPath &a) const {
      // if both values are provided and they are not the same -- they are not

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
      device_path = vendor_id,
      last_position = serial_number
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

  bool ping(
    const var::StringView path,
    IsKeepConnection is_keep_connection = IsKeepConnection::no);
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

  class File : public fs::FileAccess<File> {
  public:
    File() {}

    explicit File(
      var::StringView name,
      fs::OpenMode flags = fs::OpenMode::read_only(),
      link_transport_mdriver_t *driver = nullptr);

    File(
      IsOverwrite is_overwrite,
      var::StringView path,
      fs::OpenMode flags = fs::OpenMode::read_write(),
      fs::Permissions perms = fs::Permissions(0666),
      link_transport_mdriver_t *driver = nullptr);

    File(const File &file) = delete;
    File &operator=(const File &file) = delete;

    File(File &&a) { swap(a); }
    File &operator=(File &&a) {
      swap(a);
      return *this;
    }

    virtual ~File();

    int fileno() const;
    int flags() const;
    const File &sync() const;
    File &set_fileno(int fd) {
      m_fd = fd;
      return *this;
    }

  protected:
    int interface_lseek(int offset, int whence) const override;
    int interface_read(void *buf, int nbyte) const override;
    int interface_write(const void *buf, int nbyte) const override;
    int interface_ioctl(int request, void *argument) const override;

    int internal_fsync(int fd) const;

  private:
    API_AF(File, link_transport_mdriver_t *, driver, nullptr);
    int m_fd = -1;

    int fstat(struct stat *st);

    void internal_create(
      IsOverwrite is_overwrite,
      var::StringView path,
      fs::OpenMode open_mode,
      fs::Permissions perms);

    void open(
      var::StringView name,
      fs::OpenMode flags = fs::OpenMode::read_write(),
      fs::Permissions perms = fs::Permissions(0666));

    // open/close are part of construction/deconstruction and can't be virtual
    void close();
    int internal_close(int fd) const;
    int internal_open(const char *path, int flags, int mode) const;

    void swap(File &a) {
      std::swap(m_fd, a.m_fd);
      std::swap(m_driver, a.m_driver);
    }
  };

  class Dir : public fs::DirAccess<Dir> {
  public:
    Dir(var::StringView path, link_transport_mdriver_t *driver = nullptr);

    Dir(const Dir &dir) = delete;
    Dir &operator=(const Dir &dir) = delete;
    Dir(Dir &&dir) = default;
    Dir &operator=(Dir &&dir) = default;

    static const var::String filter_hidden(const var::String &entry) {
      if (!entry.is_empty() && entry.front() == '.') {
        return var::String();
      }
      return entry;
    }

    ~Dir();

  protected:
    Dir &open(var::StringView path);
    Dir &close();

    int interface_readdir_r(dirent *result, dirent **resultp) const override;

    int interface_closedir() const override;
    int interface_telldir() const override;
    void interface_seekdir(size_t location) const override;
    void interface_rewinddir() const override;

  private:
    API_RAC(Dir, var::PathString, path);
    API_AF(Dir, link_transport_mdriver_t *, driver, nullptr);

    DIR *m_dirp = nullptr;
    mutable struct dirent m_entry = {0};

    DIR *interface_opendir(const char *path) const;
  };

  class FileSystem : public api::ExecutionContext {
  public:
    using IsOverwrite = File::IsOverwrite;
    using IsRecursive = Dir::IsRecursive;

    FileSystem(link_transport_mdriver_t *driver);

    bool exists(var::StringView path) const;

    const FileSystem &remove(var::StringView path) const;
    const FileSystem &remove_directory(var::StringView path) const;

    const FileSystem &
    remove_directory(var::StringView path, IsRecursive recursive) const;

    bool directory_exists(var::StringView path) const;

    const FileSystem &create_directory(
      var::StringView path,
      const fs::Permissions &permissions = fs::Permissions(0)) const;

    const FileSystem &create_directory(
      var::StringView path,
      IsRecursive is_recursive,
      const fs::Permissions &permissions = fs::Permissions(0)) const;

    fs::PathList read_directory(
      const var::StringView path,
      IsRecursive is_recursive = IsRecursive::no,
      bool (*exclude)(var::StringView) = nullptr) const;

    class Rename {
      API_AC(Rename, var::StringView, source);
      API_AC(Rename, var::StringView, destination);
    };

    const FileSystem &rename(const Rename &options) const;
    inline const FileSystem &operator()(const Rename &options) const {
      return rename(options);
    }

    const FileSystem &touch(var::StringView path) const;

    fs::FileInfo get_info(var::StringView path) const;
    fs::FileInfo get_info(const File &file) const;

  protected:
    fs::Permissions get_permissions(var::StringView path) const;

    int interface_mkdir(const char *path, int mode) const;
    int interface_rmdir(const char *path) const;
    int interface_unlink(const char *path) const;
    int interface_stat(const char *path, struct stat *stat) const;
    int interface_fstat(int fd, struct stat *stat) const;
    int interface_rename(const char *old_name, const char *new_name) const;

  private:
#ifdef __link
    API_AF(FileSystem, link_transport_mdriver_t *, driver, nullptr);
#endif
  };

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
