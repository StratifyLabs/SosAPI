// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOSAPI_APPFS_HPP_
#define SOSAPI_APPFS_HPP_

#include <api/api.hpp>
#include <fs/File.hpp>
#include <var/String.hpp>

#include <sos/dev/appfs.h>

#include "Link.hpp"

namespace sos {

class AppfsFlags {
public:
  enum class Flags {
    is_default = 0,
    is_flash = APPFS_FLAG_IS_FLASH,
    is_startup = APPFS_FLAG_IS_STARTUP,
    is_authenticated = APPFS_FLAG_IS_AUTHENTICATED,
    is_replace = APPFS_FLAG_IS_REPLACE,
    is_orphan = APPFS_FLAG_IS_ORPHAN,
    is_unique = APPFS_FLAG_IS_UNIQUE,
    is_code_external = APPFS_FLAG_IS_CODE_EXTERNAL,
    is_data_external = APPFS_FLAG_IS_DATA_EXTERNAL,
    is_code_tightly_coupled = APPFS_FLAG_IS_CODE_TIGHTLY_COUPLED,
    is_data_tightly_coupled = APPFS_FLAG_IS_DATA_TIGHTLY_COUPLED
  };
};

API_OR_NAMED_FLAGS_OPERATOR(AppfsFlags, Flags)

class Appfs : public api::ExecutionContext, public AppfsFlags {
public:
  class Info : public AppfsFlags {
  public:
    Info() { memset(&m_info, 0, sizeof(m_info)); }

    explicit Info(const appfs_info_t &info) {
      memcpy(&m_info, &info, sizeof(appfs_info_t));
    }

    bool is_valid() const { return m_info.signature != 0; }

    const var::StringView id() const {
      return var::StringView(reinterpret_cast<const char *>(m_info.id));
    }

    const var::StringView name() const {
      return var::StringView(reinterpret_cast<const char *>(m_info.name));
    }

    u16 mode() const { return m_info.mode; }
    u16 version() const { return m_info.version; }

    u32 ram_size() const { return m_info.ram_size; }

    u32 o_flags() const { return m_info.o_flags; }

    u32 signature() const { return m_info.signature; }
    bool is_executable() const { return m_info.mode & 0111; }
    bool is_startup() const { return is_flag(Flags::is_startup); }
    bool is_flash() const { return is_flag(Flags::is_flash); }
    bool is_code_external() const { return is_flag(Flags::is_code_external); }
    bool is_data_external() const { return is_flag(Flags::is_data_external); }
    bool is_code_tightly_coupled() const {
      return is_flag(Flags::is_code_tightly_coupled);
    }

    bool is_data_tightly_coupled() const {
      return is_flag(Flags::is_data_tightly_coupled);
    }
    bool is_orphan() const { return is_flag(Flags::is_orphan); }
    bool is_authenticated() const { return is_flag(Flags::is_authenticated); }
    bool is_unique() const { return is_flag(Flags::is_unique); }

    const appfs_info_t &info() const { return m_info; }
    appfs_info_t &info() { return m_info; }

  private:
    appfs_info_t m_info;

    bool is_flag(Flags flags) const {
      return (static_cast<Flags>(m_info.o_flags) & flags);
    }
  };

  class FileAttributes : public AppfsFlags {
  public:
    FileAttributes(const fs::FileObject &existing);

    explicit FileAttributes(const appfs_file_t &appfs_file);

    const FileAttributes &apply(const fs::FileObject &file) const;

    Flags flags() const {
      return static_cast<Flags>(m_file_header.exec.o_flags);
    }

    u32 signature() const { return m_file_header.exec.signature; }

    bool is_flash() const { return flags() & Flags::is_flash; }
    bool is_code_external() const { return flags() & Flags::is_code_external; }
    bool is_data_external() const { return flags() & Flags::is_data_external; }
    bool is_code_tightly_coupled() const {
      return flags() & Flags::is_code_tightly_coupled;
    }
    bool is_data_tightly_coupled() const {
      return flags() & Flags::is_data_tightly_coupled;
    }
    bool is_startup() const { return flags() & Flags::is_startup; }
    bool is_unique() const { return flags() & Flags::is_unique; }
    bool is_authenticated() const { return flags() & Flags::is_authenticated; }

    FileAttributes &set_startup(bool value = true) {
      return set_flag_value(Flags::is_startup, value);
    }

    FileAttributes &set_flash(bool value = true) {
      return set_flag_value(Flags::is_flash, value);
    }

    FileAttributes &set_code_external(bool value = true) {
      return set_flag_value(Flags::is_code_external, value);
    }

    FileAttributes &set_data_external(bool value = true) {
      return set_flag_value(Flags::is_data_external, value);
    }

    FileAttributes &set_code_tightly_coupled(bool value = true) {
      return set_flag_value(Flags::is_code_tightly_coupled, value);
    }

    FileAttributes &set_data_tightly_coupled(bool value = true) {
      return set_flag_value(Flags::is_data_tightly_coupled, value);
    }

    FileAttributes &set_unique(bool value = true) {
      return set_flag_value(Flags::is_unique, value);
    }

    FileAttributes &set_authenticated(bool value = true) {
      return set_flag_value(Flags::is_authenticated, value);
    }

    var::StringView name() const { return m_file_header.hdr.name; }
    FileAttributes &set_name(const var::StringView value) {
      var::View(m_file_header.hdr.name).fill(0).copy(value);
      return *this;
    }

    var::StringView id() const { return m_file_header.hdr.id; }
    FileAttributes &set_id(const var::StringView value) {
      var::View(m_file_header.hdr.id).fill(0).copy(value);
      return *this;
    }

    u32 ram_size() const { return m_file_header.exec.ram_size; }
    FileAttributes &set_ram_size(u32 value) {
      if (value > 0) {
        m_file_header.exec.ram_size = value;
      }
      return *this;
    }

    u16 version() const { return m_file_header.hdr.version; }
    FileAttributes &set_version(u32 value) {
      m_file_header.hdr.version = value;
      return *this;
    }

    u16 access_mode() const { return m_file_header.hdr.mode; }
    FileAttributes &set_access_mode(u32 value) {
      m_file_header.hdr.mode = value;
      return *this;
    }

  private:
    void assign_flags(Flags flags) {
      m_file_header.exec.o_flags = static_cast<u32>(flags);
    }

    FileAttributes &set_flag_value(Flags flag, bool value) {
      Flags a = flags();
      if (value) {
        a |= flag;
      } else {
        a &= ~flag;
      }
      assign_flags(a);
      return *this;
    }

    appfs_file_t m_file_header;
  };

  class Construct {
  public:
    Construct() : m_mount("/app") {}

  private:
    API_ACCESS_COMPOUND(Construct, var::StringView, name);
    API_ACCESS_COMPOUND(Construct, var::StringView, mount);
    API_ACCESS_FUNDAMENTAL(Construct, u32, size, 0);
    API_ACCESS_BOOL(Construct, executable, false);
    API_ACCESS_BOOL(Construct, overwrite, false);
  };

  Appfs(const Construct &options FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);
  Appfs(FSAPI_LINK_DECLARE_DRIVER_NULLPTR);

  Appfs &append(
    const fs::FileObject &file,
    const api::ProgressCallback *progress_callback = nullptr);

  bool is_append_ready() const { return m_bytes_written < m_data_size; }

  bool is_valid() const { return m_data_size != 0; }
  u32 size() const { return m_data_size - sizeof(appfs_file_t); }

  u32 bytes_written() const { return m_bytes_written; }
  u32 bytes_available() const { return m_data_size - m_bytes_written; }

  bool is_flash_available();
  bool is_ram_available();

  static constexpr int page_size() { return APPFS_PAGE_SIZE; }
  static constexpr u32 overhead() { return sizeof(appfs_file_t); }

  Info get_info(const var::StringView path);

#if !defined __link

  enum class CleanData{no, yes};
  Appfs &cleanup(CleanData clean_data);
  Appfs &free_ram(var::StringView path);
  Appfs &reclaim_ram(var::StringView path);

#endif

private:
#if defined __link
  API_AF(Appfs, link_transport_mdriver_t *, driver, nullptr);
  Link::File m_file;
#else
  fs::File m_file;
#endif

  appfs_createattr_t m_create_install_attributes = {0};
  u32 m_bytes_written = 0;
  u32 m_data_size = 0;
  int m_request = I_APPFS_CREATE;

  void create_asynchronous(const Construct &options);
  void append(var::View blob);
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Appfs::Info &a);
Printer &operator<<(Printer &printer, const sos::Appfs::FileAttributes &a);
Printer &operator<<(Printer &printer, const appfs_file_t &a);
} // namespace printer

#endif /* SOSAPI_APPFS_HPP_ */
