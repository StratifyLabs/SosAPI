// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOSAPI_LINK_LINK_HPP_
#define SOSAPI_LINK_LINK_HPP_

#include "macros.hpp"

#if defined __link

#include <sdk/types.h>
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
    Info() = default;
    Info(var::StringView path, const sys_info_t &sys_info)
      : m_path(path), m_info(sys_info) {}

    API_NO_DISCARD bool is_valid() const { return cpu_frequency() != 0; }
    API_NO_DISCARD var::StringView id() const { return m_info.id; }
    API_NO_DISCARD var::StringView team_id() const { return m_info.team_id; }
    API_NO_DISCARD var::StringView name() const { return m_info.name; }
    API_NO_DISCARD var::StringView system_version() const { return m_info.sys_version; }
    API_NO_DISCARD var::StringView bsp_version() const { return m_info.sys_version; }
    API_NO_DISCARD var::StringView sos_version() const { return m_info.kernel_version; }
    API_NO_DISCARD var::StringView kernel_version() const { return m_info.kernel_version; }
    API_NO_DISCARD var::StringView cpu_architecture() const { return m_info.arch; }
    API_NO_DISCARD u32 cpu_frequency() const { return m_info.cpu_freq; }
    API_NO_DISCARD u32 application_signature() const { return m_info.signature; }
    API_NO_DISCARD var::StringView bsp_git_hash() const { return m_info.bsp_git_hash; }
    API_NO_DISCARD var::StringView sos_git_hash() const { return m_info.sos_git_hash; }
    API_NO_DISCARD var::StringView mcu_git_hash() const { return m_info.mcu_git_hash; }

    API_NO_DISCARD u32 o_flags() const { return m_info.o_flags; }

    API_NO_DISCARD var::StringView architecture() const { return m_info.arch; }
    API_NO_DISCARD var::StringView stdin_name() const { return m_info.stdin_name; }
    API_NO_DISCARD var::StringView stdout_name() const { return m_info.stdout_name; }
    API_NO_DISCARD var::StringView trace_name() const { return m_info.trace_name; }
    API_NO_DISCARD u32 hardware_id() const { return m_info.hardware_id; }

    API_NO_DISCARD SerialNumber serial_number() const { return SerialNumber(m_info.serial); }

    API_NO_DISCARD const sys_info_t &sys_info() const { return m_info; }

  private:
    API_ACCESS_COMPOUND(Info, var::PathString, path);
    sys_info_t m_info{};
  };

  class Path {
  public:
    Path() = default;
    Path(var::StringView path, link_transport_mdriver_t *driver) {
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

    API_NO_DISCARD bool is_valid() const { return !m_path.is_empty(); }

    static bool is_device_path(var::StringView path) {
      return path.find(device_prefix()) == 0;
    }

    static bool is_host_path(var::StringView path) {
      return path.find(host_prefix()) == 0;
    }

    API_NO_DISCARD static var::StringView device_prefix() { return "device@"; }

    API_NO_DISCARD static var::StringView host_prefix() { return "host@"; }

    API_NO_DISCARD var::PathString path_description() const {
      return (m_driver ? device_prefix() : host_prefix())
             & m_path.string_view();
    }

    API_NO_DISCARD bool is_device_path() const { return m_driver != nullptr; }
    API_NO_DISCARD bool is_host_path() const { return m_driver == nullptr; }

    API_NO_DISCARD var::StringView prefix() const {
      return is_host_path() ? host_prefix() : device_prefix();
    }

    API_NO_DISCARD var::StringView path() const { return m_path.string_view(); }

    API_NO_DISCARD link_transport_mdriver_t *driver() const { return m_driver; }

  private:
    link_transport_mdriver_t *m_driver = nullptr;
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

    DriverPath() = default;
    explicit DriverPath(const Construct &options);
    explicit DriverPath(var::StringView driver_path);

    API_NO_DISCARD bool is_valid() const;
    API_NO_DISCARD Type get_type() const;
    API_NO_DISCARD var::StringView get_driver_name() const;
    API_NO_DISCARD var::StringView get_vendor_id() const;
    API_NO_DISCARD var::StringView get_product_id() const;
    API_NO_DISCARD var::StringView get_interface_number() const;
    API_NO_DISCARD var::StringView get_serial_number() const;
    API_NO_DISCARD var::StringView get_device_path() const;
    API_NO_DISCARD bool is_partial() const;
    API_NO_DISCARD bool operator==(const DriverPath &a) const;

  private:
    API_AC(DriverPath, var::PathString, path);

    enum class Position {
      null,
      driver_name,
      vendor_id_or_device_path,
      product_id,
      interface_number,
      serial_number
    };

    static constexpr size_t last_position = size_t(Position::serial_number);

    API_NO_DISCARD var::StringView
    get_value_at_position(Position position) const;
    API_NO_DISCARD var::Tokenizer split() const;

    static size_t get_position(Position value) { return size_t(value); }

    static size_t position_count() {
      return last_position + 1;
    }

    var::String lookup_serial_port_path_from_usb_details();
  };

  Link();
  ~Link();

  fs::PathContainer get_path_list();

  using InfoList = var::Vector<Info>;
  InfoList get_info_list();

  Link &connect(var::StringView path, IsLegacy is_legacy = IsLegacy::no);
  API_NO_DISCARD bool is_legacy() const { return m_is_legacy == IsLegacy::yes; }
  Link &reconnect(int retries = 5, chrono::MicroTime delay = 500_milliseconds);
  Link &disconnect();
  Link &disregard_connection();

  bool ping(
    var::StringView path,
    IsKeepConnection is_keep_connection = IsKeepConnection::no);
  API_NO_DISCARD bool is_connected() const;

  static var::KeyString convert_permissions(link_mode_t mode);

  Link &format(var::StringView path); // Format the drive
  Link &run_app(var::StringView path);

  Link &reset();
  Link &reset_bootloader();
  Link &get_bootloader_attr(bootloader_attr_t &attr);
  API_NO_DISCARD bool is_bootloader() const { return m_is_bootloader == IsBootloader::yes; }
  bool is_signature_required();
  API_NO_DISCARD bool is_connected_and_is_not_bootloader() const {
    return is_connected() && !is_bootloader();
  }

  var::Array<u8, 64> get_public_key();

  Link &write_flash(int addr, const void *buf, int nbyte);
  Link &read_flash(int addr, void *buf, int nbyte);

  Link &get_time(struct tm *gt);
  Link &set_time(struct tm *gt);

  class UpdateOs {
    API_AF(UpdateOs, const fs::FileObject *, image, nullptr);
    API_AF(UpdateOs, u32, bootloader_retry_count, 20);
    API_AF(UpdateOs, printer::Printer *, printer, nullptr);
    API_AB(UpdateOs, verify, false);
    API_AC(UpdateOs, var::PathString, flash_path);
  };

  Link &update_os(const UpdateOs &options);
  inline Link &operator()(const UpdateOs &options) {
    return update_os(options);
  }

  API_NO_DISCARD const link_transport_mdriver_t *driver() const { return &m_driver_instance; }
  link_transport_mdriver_t *driver() { return &m_driver_instance; }

  Link &set_driver_options(const void *options) {
    m_driver_instance.options = options;
    return *this;
  }

  Link &set_driver(link_transport_mdriver_t *driver) {
    m_driver_instance = *driver;
    return *this;
  }

  API_NO_DISCARD int progress() const { return m_progress; }
  API_NO_DISCARD int progress_max() const { return m_progress_max; }

  Link &set_progress(int p) {
    m_progress = p;
    return *this;
  }

  Link &set_progress_max(int p) {
    m_progress_max = p;
    return *this;
  }

  API_NO_DISCARD const Info &info() const { return m_link_info; }

  class File : public fs::FileAccess<File> {
  public:
    File() = default;
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

    API_NO_DISCARD bool is_valid() const { return fd() >= 0; }
    API_NO_DISCARD int fileno() const;
    API_NO_DISCARD int flags() const;
    const File &sync() const;
    File &set_fileno(int fd) {
      m_file_resource.pointer_to_value()->fd = fd;
      return *this;
    }

    API_NO_DISCARD link_transport_mdriver_t *driver() const {
      return m_file_resource.value().driver;
    }

  private:
    struct FileResource {
      int fd = -1;
      link_transport_mdriver_t *driver = nullptr;
    };

    static void file_deleter(FileResource *file_resource);
    using FileSystemResource
      = api::SystemResource<FileResource, decltype(&file_deleter)>;

    FileSystemResource m_file_resource;

    API_NO_DISCARD int fd() const { return m_file_resource.value().fd; }

    FileResource open(
      var::StringView name,
      fs::OpenMode flags,
      fs::Permissions permissions,
      link_transport_mdriver_t *driver);

    FileResource create(
      IsOverwrite is_overwrite,
      var::StringView path,
      fs::OpenMode open_mode,
      fs::Permissions perms,
      link_transport_mdriver_t *driver);

    API_NO_DISCARD int interface_lseek(int offset, int whence) const override;
    API_NO_DISCARD int interface_read(void *buf, int nbyte) const override;
    API_NO_DISCARD int interface_write(const void *buf, int nbyte) const override;
    API_NO_DISCARD int interface_ioctl(int request, void *argument) const override;
    API_NO_DISCARD int internal_fsync(int fd) const;

    int fstat(struct stat *st);
  };

  class Dir : public fs::DirAccess<Dir> {
  public:
    Dir() = default;
    explicit Dir(
      var::StringView path,
      link_transport_mdriver_t *driver = nullptr);

    static var::String filter_hidden(const var::String &entry) {
      if (!entry.is_empty() && entry.front() == '.') {
        return {};
      }
      return entry;
    }

    const var::PathString &path() const {
      return m_directory_system_resource.value().path;
    }

  private:
    struct DirectoryResource {
      DIR *dirp = nullptr;
      link_transport_mdriver_t *driver = nullptr;
      var::PathString path = {};
    };

    static void directory_deleter(DirectoryResource *directory_resource);
    using DirectorySystemResource
      = api::SystemResource<DirectoryResource, decltype(&directory_deleter)>;

    DirectoryResource
    open(var::StringView path, link_transport_mdriver_t *driver);

    link_transport_mdriver_t *driver() const {
      return m_directory_system_resource.value().driver;
    }

    DIR *dirp() const { return m_directory_system_resource.value().dirp; }

    int interface_readdir_r(dirent *result, dirent **resultp) const override;
    long interface_telldir() const override;
    void interface_seekdir(size_t location) const override;
    void interface_rewinddir() const override;

    DirectorySystemResource m_directory_system_resource;

    mutable struct dirent m_entry = {};
    DIR *interface_opendir(const char *path) const;
  };

  class FileSystem : public api::ExecutionContext {
  public:
    using IsOverwrite = File::IsOverwrite;
    using IsRecursive = Dir::IsRecursive;

    explicit FileSystem(link_transport_mdriver_t *driver);

    API_NO_DISCARD bool exists(var::StringView path) const;

    const FileSystem &remove(var::StringView path) const;
    const FileSystem &remove_directory(var::StringView path) const;

    const FileSystem &
    remove_directory(var::StringView path, IsRecursive recursive) const;

    API_NO_DISCARD bool directory_exists(var::StringView path) const;

    const FileSystem &create_directory(
      var::StringView path,
      const fs::Permissions &permissions = fs::Permissions(0)) const;

    const FileSystem &create_directory(
      var::StringView path,
      IsRecursive is_recursive,
      const fs::Permissions &permissions = fs::Permissions(0)) const;

    API_NO_DISCARD fs::PathList read_directory(
      var::StringView path,
      IsRecursive is_recursive = IsRecursive::no,
      bool (*exclude)(var::StringView, void *context) = nullptr,
      void *context = nullptr) const;

    class Rename {
      API_AC(Rename, var::StringView, source);
      API_AC(Rename, var::StringView, destination);
    };

    const FileSystem &rename(const Rename &options) const;
    inline const FileSystem &operator()(const Rename &options) const {
      return rename(options);
    }

    const FileSystem &touch(var::StringView path) const;

    API_NO_DISCARD fs::FileInfo get_info(var::StringView path) const;
    API_NO_DISCARD fs::FileInfo get_info(const File &file) const;

  protected:
    API_NO_DISCARD fs::Permissions get_permissions(var::StringView path) const;

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
  enum class Connection { null, bootloader, os };

  volatile int m_progress = 0;
  volatile int m_progress_max = 0;
  IsBootloader m_is_bootloader = IsBootloader::no;
  IsLegacy m_is_legacy = IsLegacy::no;

  Info m_link_info;

  bootloader_attr_t m_bootloader_attributes = {};
  link_transport_mdriver_t m_driver_instance = {};

  enum class UseBootloaderId { no, yes };

  u32 validate_os_image_id_with_connected_bootloader(
    const fs::FileObject *source_image,
    UseBootloaderId bootloader_id = UseBootloaderId::yes);

  // these use the bootloader
  Link &erase_os(const UpdateOs &options);
  Link &install_os(u32 image_id, const UpdateOs &options);

  // these use a bootloader running a full Stratify OS instance
  void update_os_flash_device(const UpdateOs &options);
  void erase_os_flash_device(const UpdateOs &options, const File &flash_device);
  void
  install_os_flash_device(const UpdateOs &options, const File &flash_device);

  Link &reset_progress();
  Connection ping_connection(var::StringView path);
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Link::Info &a);
Printer &operator<<(Printer &printer, const sos::Link::InfoList &a);
} // namespace printer

#endif // link

#endif // SOSAPI_LINK_LINK_HPP_
