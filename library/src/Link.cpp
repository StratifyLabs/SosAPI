/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.
/* Copyright 2016-2018 Tyler Gilbert ALl Rights Reserved */

#if defined __link

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sos/dev/sys.h>
#include <sos/fs/sysfs.h>
#include <sstream>
#include <string>

#include <chrono.hpp>
#include <fs/File.hpp>
#include <fs/Path.hpp>
#include <var.hpp>

#include "sos/Appfs.hpp"
#include "sos/Link.hpp"
#include "sos/Sys.hpp"

#define MAX_TRIES 3

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Link::Info &a) {
  printer.key("path", a.path());
  printer.object("systemInformation", sos::Sys::Info(a.sys_info()));
  return printer;
}

Printer &operator<<(Printer &printer, const sos::Link::InfoList &a) {
  int i = 0;
  for (const auto &info : a) {
    printer.object(var::NumberString(i++), info);
  }
  return printer;
}
} // namespace printer

using namespace fs;
using namespace sos;

Link::Link() { link_load_default_driver(driver()); }

Link::~Link() { disconnect(); }

Link &Link::reset_progress() {
  m_progress = 0;
  m_progress_max = 0;
  return *this;
}

fs::PathList Link::get_path_list() {
  fs::PathList result;
  PathString device_name;
  PathString last_device;

  while (driver()->getname(
           device_name.to_char(),
           last_device.cstring(),
           static_cast<int>(device_name.capacity()))
         == 0) {

    // this will make a copy of device name and put it on the list
    result.push_back(device_name);
    last_device = device_name;
  }
	return result;
}

var::Vector<Link::Info> Link::get_info_list() {
  var::Vector<Info> result;
  fs::PathList path_list = get_path_list();

  // disconnect if already connected
  disconnect();

  for (const auto &path : path_list) {
    // ping and grab the info

    connect(path);

    // couldn't connect
    if (is_success()) {
      result.push_back(Info(path, info().sys_info()));
      disconnect();
    } else {
      API_RESET_ERROR();
    }
  }

  return result;
}

Link::Connection Link::ping_connection(const var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(Connection::null);
  if (driver()->phy_driver.handle == LINK_PHY_OPEN_ERROR) {
    driver()->transport_version = 0;
    const var::PathString path_string(path);

    driver()->phy_driver.handle = API_SYSTEM_CALL_NULL(
      path_string.cstring(),
      driver()->phy_driver.open(path_string.cstring(), driver()->options));

    if (is_error()) {
      disregard_connection();
      API_RETURN_VALUE_IF_ERROR(Connection::null);
    }
  }

  int err;
  if (m_is_legacy == IsLegacy::yes) {
    err = API_SYSTEM_CALL("", link_isbootloader_legacy(driver()));
  } else {
    err = API_SYSTEM_CALL("", link_isbootloader(driver()));
  }

  if (err > 0) {
    return Connection::bootloader;
  } else if (err == 0) {
    return Connection::os;
  }

  driver()->phy_driver.close(&driver()->phy_driver.handle);
  driver()->phy_driver.handle = LINK_PHY_OPEN_ERROR;
  return Connection::null;
}

Link &Link::connect(var::StringView path, IsLegacy is_legacy) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() && info().path() != path) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  reset_progress();

  m_is_legacy = is_legacy;

  Connection connection = ping_connection(path);

  switch (connection) {
  case Connection::null:
    return *this;
  case Connection::bootloader:
    m_is_bootloader = IsBootloader::yes;
    break;
  case Connection::os:
    m_is_bootloader = IsBootloader::no;
    break;
  }

  sys_info_t sys_info;
  if (m_is_bootloader == IsBootloader::no) {
    link_get_sys_info(driver(), &sys_info);
  } else {
    get_bootloader_attr(m_bootloader_attributes);
    memset(&sys_info, 0, sizeof(sys_info));
    strcpy(sys_info.name, "bootloader");
    sys_info.hardware_id = m_bootloader_attributes.hardware_id;
    memcpy(
      &sys_info.serial,
      m_bootloader_attributes.serialno,
      sizeof(mcu_sn_t));
  }

  m_link_info = Info(path, sys_info);
  return *this;
}

Link &Link::reconnect(int retries, chrono::MicroTime delay) {
  API_RETURN_VALUE_IF_ERROR(*this);
  Info last_info(info());
  for (u32 i = 0; i < retries; i++) {
    connect(last_info.path());

    if (is_success()) {
      if (last_info.serial_number() == info().serial_number()) {
        return *this;
      }
      disconnect();
    } else {
      API_RESET_ERROR();
    }

    fs::PathList port_list = get_path_list();
    for (u32 j = 0; j < port_list.count(); j++) {
      connect(port_list.at(j));

      if (is_success()) {
        if (last_info.serial_number() == info().serial_number()) {
          return *this;
        }
        disconnect();
      } else {
        API_RESET_ERROR();
      }
    }

    chrono::wait(delay);
  }

  // restore the last known information on failure
  m_link_info = last_info;

  return *this;
}

Link &Link::read_flash(int addr, void *buf, int nbyte) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_readflash(driver(), addr, buf, nbyte);
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

Link &Link::write_flash(int addr, const void *buf, int nbyte) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_writeflash(driver(), addr, buf, nbyte);
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

Link &Link::disconnect() {

  if (driver()->phy_driver.handle != LINK_PHY_OPEN_ERROR) {
    link_disconnect(driver());

    if (driver()->phy_driver.handle != LINK_PHY_OPEN_ERROR) {
      // can't unlock the device if it has been destroyed
    }
  }

  m_is_bootloader = IsBootloader::no;
  return *this;
}

Link &Link::disregard_connection() {
  driver()->transport_version = 0;
  driver()->phy_driver.handle = LINK_PHY_OPEN_ERROR;
  return *this;
}

bool Link::is_connected() const {
  if (driver()->phy_driver.handle == LINK_PHY_OPEN_ERROR) {
    return false;
  }
  return true;
}

bool Link::ping(
  const var::StringView path,
  IsKeepConnection is_keep_connection) {
  API_RETURN_VALUE_IF_ERROR(false);
  Connection connection = ping_connection(path);
  if (connection == Connection::null) {
    API_RESET_ERROR();
    return false;
  }
  if (is_keep_connection == IsKeepConnection::no) {
    disconnect();
  }
  return true;
}

Link &Link::get_time(struct tm *gt) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  struct link_tm ltm;
  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_gettime(driver(), &ltm);
    if (err != LINK_PROT_ERROR)
      break;
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", link_errno);
  } else {
    gt->tm_hour = ltm.tm_hour;
    gt->tm_isdst = ltm.tm_isdst;
    gt->tm_mday = ltm.tm_mday;
    gt->tm_min = ltm.tm_min;
    gt->tm_mon = ltm.tm_mon;
    gt->tm_sec = ltm.tm_sec;
    gt->tm_wday = ltm.tm_wday;
    gt->tm_yday = ltm.tm_yday;
    gt->tm_year = ltm.tm_year;
  }
  return *this;
}

Link &Link::set_time(struct tm *gt) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  struct link_tm ltm;

  ltm.tm_hour = gt->tm_hour;
  ltm.tm_isdst = gt->tm_isdst;
  ltm.tm_mday = gt->tm_mday;
  ltm.tm_min = gt->tm_min;
  ltm.tm_mon = gt->tm_mon;
  ltm.tm_sec = gt->tm_sec;
  ltm.tm_wday = gt->tm_wday;
  ltm.tm_yday = gt->tm_yday;
  ltm.tm_year = gt->tm_year;

  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
    return *this;
  }

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_settime(driver(), &ltm);
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

var::KeyString Link::convert_permissions(link_mode_t mode) {
  API_RETURN_VALUE_IF_ERROR(var::KeyString());
  var::KeyString result;

  link_mode_t type;
  type = mode & LINK_S_IFMT;
  switch (type) {
  case LINK_S_IFDIR:
    result = "d";
    break;
  case LINK_S_IFCHR:
    result = "c";
    break;
  case LINK_S_IFBLK:
    result = "b";
    break;
  case LINK_S_IFLNK:
    result = "l";
    break;
  case LINK_S_IFREG:
    result = "-";
    break;
  default:
    result = "x";
  }

  if (mode & LINK_S_IROTH) {
    result.append("r");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IWOTH) {
    result.append("w");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IXOTH) {
    result.append("x");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IRGRP) {
    result.append("r");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IWGRP) {
    result.append("w");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IXGRP) {
    result.append("x");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IRUSR) {
    result.append("r");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IWUSR) {
    result.append("w");
  } else {
    result.append("-");
  }

  if (mode & LINK_S_IXUSR) {
    result.append("x");
  } else {
    result.append("-");
  }

  return result;
}

Link &Link::run_app(const var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  if (path.length() >= LINK_PATH_ARG_MAX - 1) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_exec(driver(), PathString(path).cstring());
    if (err != LINK_PROT_ERROR)
      break;
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", link_errno);
  }

  return *this;
}

Link &Link::format(const var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }
  // Format the filesystem

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_mkfs(driver(), PathString(path).cstring());
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

Link &Link::reset() {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  // this will always result in an error (even on success)
  link_reset(driver());
  API_RESET_ERROR();

  disregard_connection();

  return *this;
}

Link &Link::reset_bootloader() {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  link_resetbootloader(driver());
  API_RESET_ERROR();
  disregard_connection();
  return *this;
}

Link &Link::get_bootloader_attr(bootloader_attr_t &attr) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  int err = -1;
  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  if (is_legacy()) {
    err = link_bootloader_attr_legacy(driver(), &attr, 0);
  } else {
    err = link_bootloader_attr(driver(), &attr, 0);
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  return *this;
}

u32 Link::validate_os_image_id_with_connected_bootloader(
  const FileObject *source_image) {
  API_RETURN_VALUE_IF_ERROR(0);
  int err = -1;
  u32 image_id;

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(0, "", EBADF);
  }

  if (!is_bootloader()) {
    return 0;
  }

  // now write the OS to the device using link_writeflash()
  m_progress_max = 0;
  m_progress = 0;

  if (source_image->seek(BOOTLOADER_HARDWARE_ID_OFFSET)
        .read(var::View(image_id))
        .seek(0)
        .is_error()) {
    return 0;
  }

  m_progress_max = static_cast<int>(source_image->size());

  if ((image_id & ~0x01) != (m_bootloader_attributes.hardware_id & ~0x01)) {
    err = -1;
    return 0;
  }

  return image_id;
}

Link &Link::erase_os(const UpdateOs &options) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  const api::ProgressCallback *progress_callback
    = options.printer()->progress_callback();

  options.printer()->set_progress_key("erasing");


  // first erase the flash
  API_SYSTEM_CALL("", link_eraseflash(driver()));
  API_RETURN_VALUE_IF_ERROR(*this);

  if (progress_callback) {
    progress_callback->update(
      0,
      api::ProgressCallback::indeterminate_progress_total());
  }

  bootloader_attr_t attr = {0};
  int retry = 0;
  bool is_waiting = true;
  do {
    API_RESET_ERROR();
    chrono::wait(500_milliseconds);
    get_bootloader_attr(attr);
    if (is_error()) {
      API_RESET_ERROR();
      driver()->phy_driver.flush(driver()->phy_driver.handle);
    } else {
      is_waiting = false;
    }

    if (progress_callback) {
      progress_callback->update(
        retry,
        api::ProgressCallback::indeterminate_progress_total());
    }
  } while (is_waiting && (retry++ < options.bootloader_retry_count()));

  const bool is_error_state = is_error();

  chrono::wait(250_milliseconds);
  // flush just incase the protocol gets filled with get attr requests
  driver()->phy_driver.flush(driver()->phy_driver.handle);

  if (progress_callback) {
    progress_callback->update(0, 0);
  }

  if (is_error_state) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  return *this;
}

Link &Link::install_os(u32 image_id, const UpdateOs &options) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  // must be connected to the bootloader with an erased OS
  int err = -1;
  const int buffer_size = 1024;

  const api::ProgressCallback *progress_callback
    = options.printer()->progress_callback();

  var::Data start_address_buffer(256);
  var::Data buffer(buffer_size);
  var::Data compare_buffer(buffer_size);

  if (options.image()->seek(0).is_error()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  u32 start_address = m_bootloader_attributes.startaddr;
  u32 loc = start_address;

  options.printer()->set_progress_key("installing");

  if (progress_callback) {
    progress_callback->update(0, 100);
  }

  var::View(buffer).fill<u8>(0xff);

  bootloader_attr_t attr;
  get_bootloader_attr(attr);

  API_RETURN_VALUE_IF_ERROR(*this);

  while (options.image()->read(buffer).return_value() > 0) {
    const int bytes_read = options.image()->return_value();
    if (loc == start_address) {
      // we want to write the first 256 bytes last because the bootloader checks
      // this for a valid image
      var::View buffer_view
        = var::View(buffer).truncate(start_address_buffer.size());
      start_address_buffer.copy(buffer_view);

      // memcpy(stackaddr, buffer, 256);

      buffer_view.fill<u8>(0xff);
    }

    if (
      (err = link_writeflash(driver(), loc, buffer.data(), bytes_read))
      != bytes_read) {

      if (err < 0) {
        err = -1;
      }
      break;
    }

    loc += bytes_read;
    m_progress += bytes_read;
    if (
      progress_callback
      && (progress_callback->update(m_progress, m_progress_max) == true)) {
      break;
    }
    err = 0;
  }

  if (err == 0) {

    if (options.is_verify()) {

      options.image()->seek(0);
      loc = start_address;
      m_progress = 0;

      options.printer()->progress_key() = "verifying";

      while ((options.image()->read(buffer).return_value()) > 0) {
        const int bytes_read = options.image()->return_value();

        if (
          (err
           = link_readflash(driver(), loc, compare_buffer.data(), bytes_read))
          != bytes_read) {
          if (err > 0) {
            err = -1;
          }
          break;

        } else {

          if (loc == start_address) {
            var::View(buffer)
              .truncate(start_address_buffer.size())
              .fill<u8>(0xff);
          }

          compare_buffer.resize(bytes_read);
          buffer.resize(bytes_read);

          if (var::View(compare_buffer) != var::View(buffer)) {
            API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
          }

          loc += bytes_read;
          m_progress += bytes_read;
          if (
            progress_callback
            && (progress_callback->update(m_progress, m_progress_max) == true)) {
            break;
          }
          err = 0;
        }
      }
    }

    if (image_id != m_bootloader_attributes.hardware_id) {
      // if the LSb of the image_id doesn't match the bootloader HW ID, this
      // will correct it
      memcpy(
        start_address_buffer.data_u8() + BOOTLOADER_HARDWARE_ID_OFFSET,
        &m_bootloader_attributes.hardware_id,
        sizeof(u32));
    }

    // write the start block
    if (
      (err = link_writeflash(
         driver(),
         start_address,
         start_address_buffer.data(),
         start_address_buffer.size()))
      != start_address_buffer.size()) {

      if (progress_callback) {
        progress_callback->update(0, 0);
      }

      link_eraseflash(driver());
      API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
    }

    if (options.is_verify()) {
      // verify the stack address
      buffer.resize(start_address_buffer.size());
      if (
        (err = link_readflash(
           driver(),
           start_address,
           buffer.data(),
           start_address_buffer.size()))
        != start_address_buffer.size()) {
        if (progress_callback) {
          progress_callback->update(0, 0);
        }

        API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
      }

      if (var::View(buffer) != var::View(start_address_buffer)) {
        if (progress_callback) {
          progress_callback->update(0, 0);
        }
        link_eraseflash(driver());
        API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
      }
    }
  }

  if (progress_callback) {
    progress_callback->update(0, 0);
  }

  return *this;
}

Link &Link::update_os(const UpdateOs &options) {
  API_ASSERT(options.image() != nullptr);
  API_ASSERT(options.printer() != nullptr);

  API_RETURN_VALUE_IF_ERROR(*this);
  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EBADF);
  }

  if (is_bootloader() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  u32 image_id
    = validate_os_image_id_with_connected_bootloader(options.image());

  if (image_id == 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  const var::KeyString progress_key = options.printer()->progress_key();

  erase_os(options);
  install_os(image_id, options);

  options.printer()->set_progress_key(progress_key);
  return *this;
}

var::String Link::DriverPath::lookup_serial_port_path_from_usb_details() {

#if defined __win32

#endif
  return var::String();
}

Link::File::File(
  var::StringView name,
  fs::OpenMode flags,
  link_transport_mdriver_t *driver) {
  set_driver(driver);
  open(name, flags);
}

Link::File::File(
  IsOverwrite is_overwrite,
  var::StringView path,
  fs::OpenMode open_mode,
  fs::Permissions perms,
  link_transport_mdriver_t *driver) {
  set_driver(driver);
  internal_create(is_overwrite, path, open_mode, perms);
}

Link::File::~File() {
  if (fileno() >= 0) {
    close();
  }
}

int Link::File::fileno() const { return m_fd; }

const Link::File &Link::File::sync() const {
  API_RETURN_VALUE_IF_ERROR(*this);
#if defined __link
  if (driver()) {
    return *this;
  }
#endif
  if (m_fd >= 0) {
#if !defined __win32
    API_SYSTEM_CALL("", internal_fsync(m_fd));
#endif
  }
  return *this;
}

int Link::File::flags() const {
  API_RETURN_VALUE_IF_ERROR(-1);
#if defined __link
  return -1;
#else
  if (fileno() < 0) {
    API_SYSTEM_CALL("", -1);
    return return_value();
  }
  return _global_impure_ptr->procmem_base->open_file[m_fd].flags;
#endif
}

int Link::File::fstat(struct stat *st) {
  API_RETURN_VALUE_IF_ERROR(-1);
  return API_SYSTEM_CALL("", link_fstat(driver(), m_fd, st));
}

void Link::File::close() {
  if (m_fd >= 0) {
    internal_close(m_fd);
    m_fd = -1;
  }
}

int Link::File::internal_open(const char *path, int flags, int mode) const {
  return link_open(driver(), path, flags, mode);
}

int Link::File::interface_read(void *buf, int nbyte) const {
  return link_read(driver(), m_fd, buf, nbyte);
}

int Link::File::interface_write(const void *buf, int nbyte) const {
  return link_write(driver(), m_fd, buf, nbyte);
}

int Link::File::interface_ioctl(int request, void *argument) const {
  return link_ioctl(driver(), m_fd, request, argument);
}

int Link::File::internal_close(int fd) const {
  return link_close(driver(), fd);
}

int Link::File::internal_fsync(int fd) const {
#if defined __link
  return 0;
#else
  return ::fsync(fd);
#endif
}

int Link::File::interface_lseek(int offset, int whence) const {
  return link_lseek(driver(), m_fd, offset, whence);
}

void Link::File::open(
  var::StringView path,
  OpenMode flags,
  Permissions permissions) {
  API_ASSERT(m_fd == -1);
  API_RETURN_IF_ERROR();
  const var::PathString path_string(path);
  API_SYSTEM_CALL(
    path_string.cstring(),
    m_fd = internal_open(
      path_string.cstring(),
      static_cast<int>(flags.o_flags()),
      permissions.permissions()));
}

void Link::File::internal_create(
  IsOverwrite is_overwrite,
  var::StringView path,
  OpenMode open_mode,
  Permissions perms) {
  OpenMode flags = OpenMode(open_mode).set_create();
  if (is_overwrite == IsOverwrite::yes) {
    flags.set_truncate();
  } else {
    flags.set_exclusive();
  }

  open(path, flags, perms);
}

Link::Dir::Dir(var::StringView path, link_transport_mdriver_t *driver) {
  set_driver(driver);
  open(path);
}

Link::Dir::~Dir() { close(); }

Link::Dir &Link::Dir::open(var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(*this);
  const var::PathString path_string(path);
  m_dirp = API_SYSTEM_CALL_NULL(
    path_string.cstring(),
    interface_opendir(path_string.cstring()));
  if (m_dirp) {
    m_path = path_string;
  }
  return *this;
}

Link::Dir &Link::Dir::close() {
  API_RETURN_VALUE_IF_ERROR(*this);
  if (m_dirp) {
    API_SYSTEM_CALL(m_path.cstring(), interface_closedir());
    m_dirp = nullptr;
  }

  m_path.clear();
  return *this;
}

DIR *Link::Dir::interface_opendir(const char *path) const {
  return reinterpret_cast<DIR *>(link_opendir(driver(), path));
}

int Link::Dir::interface_readdir_r(
  struct dirent *result,
  struct dirent **resultp) const {
  return link_readdir_r(driver(), m_dirp, result, resultp);
}

int Link::Dir::interface_closedir() const {
  return link_closedir(driver(), m_dirp);
}

int Link::Dir::interface_telldir() const {
  return link_telldir(driver(), m_dirp);
}

void Link::Dir::interface_seekdir(size_t location) const {
  link_seekdir(driver(), m_dirp, location);
}

void Link::Dir::interface_rewinddir() const {
  link_rewinddir(driver(), m_dirp);
}

Link::FileSystem::FileSystem(link_transport_mdriver_t *driver) {
  set_driver(driver);
}

const Link::FileSystem &Link::FileSystem::remove(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  const var::PathString path_string(path);
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_unlink(path_string.cstring()));
  return *this;
}

const Link::FileSystem &Link::FileSystem::touch(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  char c;
  API_SYSTEM_CALL(
    "",
    File(path, OpenMode::read_write(), driver())
      .read(var::View(c))
      .seek(0)
      .write(var::View(c))
      .return_value());
  return *this;
}

const Link::FileSystem &Link::FileSystem::rename(const Rename &options) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  const var::PathString source_string(options.source());
  const var::PathString dest_string(options.destination());
  API_SYSTEM_CALL(
    source_string.cstring(),
    interface_rename(source_string.cstring(), dest_string.cstring()));
  return *this;
}

bool Link::FileSystem::exists(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(false);
  bool result = File(path, OpenMode::read_only(), driver()).is_success();
  reset_error();
  return result;
}

fs::FileInfo Link::FileSystem::get_info(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(FileInfo());
  const var::PathString path_string(path);
  struct stat stat = {0};
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_stat(path_string.cstring(), &stat));
  return FileInfo(stat);
}

fs::FileInfo Link::FileSystem::get_info(const File &file) const {
  API_RETURN_VALUE_IF_ERROR(FileInfo());
  struct stat stat = {0};
  API_SYSTEM_CALL("", interface_fstat(file.fileno(), &stat));
  return FileInfo(stat);
}

const Link::FileSystem &Link::FileSystem::remove_directory(
  var::StringView path,
  IsRecursive recursive) const {

  if (recursive == IsRecursive::yes) {
    Dir d(path);

    var::String entry;
    while ((entry = d.read()).is_empty() == false) {
      var::String entry_path = path + "/" + entry;
      FileInfo info = get_info(entry_path);
      if (info.is_directory()) {
        if (entry != "." && entry != "..") {
          if (remove_directory(entry_path, recursive).is_error()) {
            return *this;
          }
        }

      } else {
        if (remove(entry_path).is_error()) {
          return *this;
        }
      }
    }
  }

  remove_directory(path);
  return *this;
}

const Link::FileSystem &
Link::FileSystem::remove_directory(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  const var::PathString path_string(path);
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_rmdir(path_string.cstring()));
  return *this;
}

PathList Link::FileSystem::read_directory(
  const var::StringView path,
  IsRecursive is_recursive,
  bool (*exclude)(var::StringView entry)) const {
  PathList result;
  bool is_the_end = false;

  Link::Dir directory(path, driver());

  do {
    const char *entry_result = directory.read();
    const var::PathString entry = (entry_result != nullptr)
                                    ? var::PathString(entry_result)
                                    : var::PathString();

    if (entry.is_empty()) {
      is_the_end = true;
    }

    if (
      (exclude == nullptr || !exclude(entry.string_view())) && !entry.is_empty()
      && (entry.string_view() != ".") && (entry.string_view() != "..")) {

      if (is_recursive == IsRecursive::yes) {

        const var::PathString entry_path
          = var::PathString(directory.path()) / entry.string_view();
        FileInfo info = get_info(entry_path.cstring());

        if (info.is_directory()) {
          PathList intermediate_result
            = read_directory(entry_path, is_recursive, exclude);

          for (const auto &intermediate_entry : intermediate_result) {
            const var::PathString intermediate_path
              = var::PathString(entry_path) / intermediate_entry;

            result.push_back(intermediate_path);
          }
        } else {
          result.push_back(entry_path);
        }
      } else {
        result.push_back(entry);
      }
    }
  } while (!is_the_end);

  return std::move(result);
}

bool Link::FileSystem::directory_exists(var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(false);
  bool result = Dir(path).is_success();
  reset_error();
  return result;
}

fs::Permissions Link::FileSystem::get_permissions(var::StringView path) const {
  const var::StringView parent = fs::Path::parent_directory(path);
  if (parent.is_empty()) {
    return get_info(".").permissions();
  }

  return get_info(parent).permissions();
}

const Link::FileSystem &Link::FileSystem::create_directory(
  var::StringView path,
  const Permissions &permissions) const {

  const Permissions use_permissions
    = permissions.permissions() == 0 ? get_permissions(path) : permissions;

  const var::PathString path_string(path);
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_mkdir(path_string.cstring(), use_permissions.permissions()));
  return *this;
}

const Link::FileSystem &Link::FileSystem::create_directory(
  var::StringView path,
  IsRecursive is_recursive,
  const Permissions &permissions) const {

  if (is_recursive == IsRecursive::no) {
    return create_directory(path, permissions);
  }

  var::Tokenizer path_tokens
    = var::Tokenizer(path, var::Tokenizer::Construct().set_delimeters("/"));
  var::String base_path;

  // tokenizer will strip the first / and create an empty token
  if (path.length() && path.front() == '/') {
    base_path += "/";
  }

  for (u32 i = 0; i < path_tokens.count(); i++) {
    if (path_tokens.at(i).is_empty() == false) {
      base_path += path_tokens.at(i);
      if (create_directory(base_path.cstring(), permissions).is_error()) {
        return *this;
      }
      base_path += "/";
    }
  }

  return *this;
}

int Link::FileSystem::interface_mkdir(const char *path, int mode) const {
  return link_mkdir(driver(), path, mode);
}

int Link::FileSystem::interface_rmdir(const char *path) const {
  return link_rmdir(driver(), path);
}

int Link::FileSystem::interface_unlink(const char *path) const {
  return link_unlink(driver(), path);
}

int Link::FileSystem::interface_stat(const char *path, struct stat *stat)
  const {
  return link_stat(driver(), path, stat);
}

int Link::FileSystem::interface_fstat(int fd, struct stat *stat) const {
  return link_fstat(driver(), fd, stat);
}

int Link::FileSystem::interface_rename(
  const char *old_name,
  const char *new_name) const {
  return link_rename(driver(), old_name, new_name);
}

#else
int sos_api_link_unused;
#endif
