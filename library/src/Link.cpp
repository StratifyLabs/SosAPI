// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#if defined __link

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sos/dev/sys.h>
#include <sos/fs/sysfs.h>
#include <sstream>
#include <string>

#include <chrono.hpp>
#include <fs/DataFile.hpp>
#include <fs/File.hpp>
#include <fs/Path.hpp>
#include <fs/ViewFile.hpp>
#include <var.hpp>

#include "sos/Appfs.hpp"
#include "sos/Auth.hpp"
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

Link::~Link() {
  api::ErrorGuard error_guard;
  disconnect();
}

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
           device_name.data(),
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
  API_RETURN_VALUE_IF_ERROR(result);

  const auto path_list = get_path_list();

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
    API_RETURN_VALUE_ASSIGN_ERROR(
      *this,
      "already connected to a different path",
      EINVAL);
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
  for (int i = 0; i < retries; i++) {
    connect(last_info.path());

    if (is_success()) {
      if (last_info.serial_number() == info().serial_number()) {
        return *this;
      }
      disconnect();
    } else {
      API_RESET_ERROR();
    }

    const auto port_list = get_path_list();
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

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    const int err = link_readflash(driver(), addr, buf, nbyte);
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

Link &Link::write_flash(int addr, const void *buf, int nbyte) {
  API_RETURN_VALUE_IF_ERROR(*this);

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    const int err = link_writeflash(driver(), addr, buf, nbyte);
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
    API_RETURN_VALUE_ASSIGN_ERROR(
      *this,
      "cannot get time from the bootloader",
      EIO);
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
    API_RETURN_VALUE_ASSIGN_ERROR(
      *this,
      "cannot set time of the bootloader",
      EINVAL);
    return *this;
  }

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    const int err = link_settime(driver(), &ltm);
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
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "bootloader is running", EIO);
  }

  if (path.length() >= LINK_PATH_ARG_MAX - 1) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "path length is too long", EINVAL);
  }

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_exec(driver(), PathString(path).cstring());
    if (err != LINK_PROT_ERROR)
      break;
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "failed to execute", link_errno);
  }

  return *this;
}

Link &Link::format(const var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(*this);
  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "bootloader is running", EIO);
  }
  // Format the filesystem

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    const int err = link_mkfs(driver(), PathString(path).cstring());
    if (err != LINK_PROT_ERROR)
      break;
  }

  return *this;
}

Link &Link::reset() {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "not connected", EBADF);
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
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "not connected", EBADF);
  }

  link_resetbootloader(driver());
  API_RESET_ERROR();
  disregard_connection();
  return *this;
}

Link &Link::get_bootloader_attr(bootloader_attr_t &attr) {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "not connected", EBADF);
  }

  int err = -1;
  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "bootloader is running", EIO);
  }

  if (is_legacy()) {
    err = link_bootloader_attr_legacy(driver(), &attr, 0);
  } else {
    err = link_bootloader_attr(driver(), &attr, 0);
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(
      *this,
      "failed to get bootloader attributes",
      EIO);
  }

  return *this;
}

u32 Link::validate_os_image_id_with_connected_bootloader(
  const FileObject *source_image,
  UseBootloaderId use_bootloader_info) {
  API_RETURN_VALUE_IF_ERROR(0);
  u32 image_id;

  if (is_connected() == false) {
    API_RETURN_VALUE_ASSIGN_ERROR(0, "not connected", EBADF);
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

  const u32 hardware_id = use_bootloader_info == UseBootloaderId::yes
                            ? m_bootloader_attributes.hardware_id
                            : m_link_info.hardware_id();

  if ((image_id & ~0x01) != (hardware_id & ~0x01)) {
    API_RETURN_VALUE_ASSIGN_ERROR(
      0,
      GeneralString().format(
        "invalid image id binary:0x%08lx bootloader:0x%08lx",
        image_id,
        hardware_id),
      EINVAL);
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

  bootloader_attr_t attr = {};
  u32 retry = 0;
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

bool Link::is_signature_required() {
  if (is_connected() == false) {
    return false;
  }

  if (is_bootloader()) {
    bootloader_attr_t attr;
    get_bootloader_attr(attr);
    return link_is_signature_required(driver(), &attr);
  }

  return false;
}

var::Array<u8, 64> Link::get_public_key() {
  var::Array<u8, 64> result;
  result.fill(0);

  if (is_bootloader()) {
    bootloader_attr_t attr;
    get_bootloader_attr(attr);
    link_get_public_key(driver(), &attr, result.data(), result.count());
  }

  return result;
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

  var::Array<u8, 256> start_address_buffer;
  // var::Data start_address_buffer(256);
  var::Data buffer(buffer_size);
  var::Data compare_buffer(buffer_size);

  if (options.image()->seek(0).is_error()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  fs::DataFile image_file = fs::DataFile().write(*options.image()).move();

  u32 start_address = m_bootloader_attributes.startaddr;
  u32 loc = start_address;

  options.printer()->set_progress_key("installing");

  if (progress_callback) {
    progress_callback->update(0, 100);
  }

  var::View(buffer).fill<u8>(0xff);

  bootloader_attr_t attr;
  get_bootloader_attr(attr);
  const int is_signature_required = link_is_signature_required(driver(), &attr);

  fs::ViewFile image = fs::ViewFile(image_file.data()).move();

  auth_signature_t signature = {};
  auth_signature_marker_t signature_marker;
  if (is_signature_required) {
    // the signature is the last 64 bytes of the OS image
    // the signature is NOT written to the device
    const auto signature_location
      = image.size() - sizeof(auth_signature_marker_t);
    image.seek(signature_location).read(var::View(signature_marker)).seek(0);

    memcpy(signature.data, signature_marker.signature.data, 64);

    image
      = fs::ViewFile(var::View(image_file.data()).truncate(signature_location))
          .move();
  }

  API_RETURN_VALUE_IF_ERROR(*this);

  while (image.read(buffer).return_value() > 0) {
    const int bytes_read = image.return_value();

    // Version 0x400 and beyond will cache the first page
    // so the sending side does not need to
    if ((attr.version < 0x400) && (loc == start_address)) {
      // we want to write the first 256 bytes last because the bootloader checks
      // this for a valid image
      var::View buffer_view
        = var::View(buffer).truncate(start_address_buffer.count());
      var::View(start_address_buffer).copy(buffer_view);
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
      && (progress_callback->update(m_progress, m_progress_max) == api::ProgressCallback::IsAbort::yes)) {
      break;
    }
    err = 0;
  }

  if (err == 0) {

    // this is called even if the signature
    // does not require verification
    // this triggers the target to install
    // the first flash page
    link_verify_signature(driver(), &attr, &signature);

    // the signature implicitly verifies the code
    // AND bootloaders that require a signature
    // do not allow reading back code
    if (options.is_verify() && !is_signature_required) {

      options.image()->seek(0);
      loc = start_address;
      m_progress = 0;

      options.printer()->progress_key() = StringView("verifying");

      while ((options.image()->read(buffer).return_value()) > 0) {
        const int bytes_read = options.image()->return_value();

        if (
          (err
           = link_readflash(driver(), loc, compare_buffer.data(), bytes_read))
          != bytes_read) {
          if (err > 0) {
            if (progress_callback) {
              progress_callback->update(0, 0);
            }
            API_RETURN_VALUE_ASSIGN_ERROR(
              *this,
              "failed to write flash when installing OS with result "
                | get_device_result_error(err),
              EIO);
          }
          break;

        } else {

          if (loc == start_address) {
            var::View(buffer)
              .truncate(start_address_buffer.count())
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
            && (progress_callback->update(m_progress, m_progress_max) == api::ProgressCallback::IsAbort::yes)) {
            break;
          }
        }
      }
    }

    if (image_id != m_bootloader_attributes.hardware_id) {
      // if the LSb of the image_id doesn't match the bootloader HW ID, this
      // will correct it

      u32 hardward_id = m_bootloader_attributes.hardware_id;
      var::View(start_address_buffer)
        .pop_front(BOOTLOADER_HARDWARE_ID_OFFSET)
        .copy(var::View(hardward_id));
    }

    // write the start block
    if (attr.version < 0x400) {
      if (int result = link_writeflash(
            driver(),
            start_address,
            start_address_buffer.data(),
            start_address_buffer.count());
          result != (int)start_address_buffer.count()) {

        if (progress_callback) {
          progress_callback->update(0, 0);
        }

        link_eraseflash(driver());

        API_RETURN_VALUE_ASSIGN_ERROR(
          *this,
          "failed to write last flash page when installing OS with result "
            | get_device_result_error(result),
          EIO);
      }
    }

    if (options.is_verify()) {
      // verify the stack address
      buffer.resize(start_address_buffer.count());
      if (
        link_readflash(
          driver(),
          start_address,
          buffer.data(),
          start_address_buffer.count())
        != (int)start_address_buffer.count()) {
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
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "not connected", EBADF);
  }

  if (is_bootloader() == false) {
    if (options.flash_path().is_empty() == false) {
      update_os_flash_device(options);
      return *this;
    }

    API_RETURN_VALUE_ASSIGN_ERROR(*this, "not bootloader", EINVAL);
  }

  u32 image_id
    = validate_os_image_id_with_connected_bootloader(options.image());
  API_RETURN_VALUE_IF_ERROR(*this);

  const var::KeyString progress_key = options.printer()->progress_key();

  erase_os(options);
  install_os(image_id, options);

  options.printer()->set_progress_key(progress_key);
  return *this;
}

void Link::update_os_flash_device(const UpdateOs &options) {

  validate_os_image_id_with_connected_bootloader(
    options.image(),
    UseBootloaderId::no);

  API_RETURN_IF_ERROR();

  const var::KeyString progress_key = options.printer()->progress_key();

  Link::File flash_device(
    options.flash_path(),
    OpenMode::read_write(),
    driver());
  API_RETURN_IF_ERROR();

  erase_os_flash_device(options, flash_device);
  install_os_flash_device(options, flash_device);

  options.printer()->set_progress_key(progress_key);
}

void Link::erase_os_flash_device(
  const UpdateOs &options,
  const Link::File &flash_device) {

  API_RETURN_IF_ERROR();

  const flash_os_info_t os_info = [&]() {
    flash_os_info_t result = {};
    flash_device.ioctl(I_FLASH_GETOSINFO, &result);
    return result;
  }();

  u32 size_erased = 0;
  int page = flash_device.ioctl(I_FLASH_GET_PAGE, MCU_INT_CAST(os_info.start))
               .return_value();

  const api::ProgressCallback *progress_callback
    = options.printer()->progress_callback();

  options.printer()->set_progress_key("erasing");

  if (progress_callback) {
    progress_callback->update(
      0,
      api::ProgressCallback::indeterminate_progress_total());
  }

  do {
    flash_pageinfo_t page_info = {};
    page_info.page = page;
    flash_device.ioctl(I_FLASH_GETPAGEINFO, &page_info);
    size_erased += page_info.size;

    link_transport_mastersettimeout(driver(), 5000);
    flash_device.ioctl(I_FLASH_ERASEPAGE, MCU_INT_CAST(page));
    link_transport_mastersettimeout(driver(), 0);

    page++;

    if (progress_callback) {
      progress_callback->update(
        page,
        api::ProgressCallback::indeterminate_progress_total());
    }

  } while (size_erased < options.image()->size());

  if (progress_callback) {
    progress_callback->update(0, 0);
  }
}

void Link::install_os_flash_device(
  const UpdateOs &options,
  const File &flash_device) {

  API_RETURN_IF_ERROR();

  const flash_os_info_t os_info = [&]() {
    flash_os_info_t result = {};
    flash_device.ioctl(I_FLASH_GETOSINFO, &result);
    return result;
  }();

  const api::ProgressCallback *progress_callback
    = options.printer()->progress_callback();

  if (progress_callback) {
    options.printer()->set_progress_key("installing");
  }

  const bool is_signature_required
    = flash_device.ioctl(I_FLASH_IS_SIGNATURE_REQUIRED).return_value()
      == 1;

  const u32 image_size = options.image()->size();
  const u32 install_size = is_signature_required
                             ? image_size - sizeof(auth_signature_marker_t)
                             : image_size;
  u32 size_processed = 0;

  do {
    flash_writepage_t write_page;
    const auto size_left = install_size - size_processed;
    const u32 page_size
      = size_left > sizeof(write_page.buf) ? sizeof(write_page.buf) : size_left;

    options.image()->read(var::View(write_page.buf, page_size));
    write_page.addr = os_info.start + size_processed;
    write_page.nbyte = page_size;
    flash_device.ioctl(I_FLASH_WRITEPAGE, &write_page);
    if (is_error()) {
      break;
    }

    size_processed += page_size;

    if (progress_callback) {
      progress_callback->update(size_processed, install_size);
    }

  } while (size_processed < install_size);

  if (is_signature_required) {
    const auto signature_info
      = Auth::get_signature_info(options.image()->seek(0));
    auth_signature_t signature = {};
    View(signature).copy(signature_info.signature().data());
    flash_device.ioctl(I_FLASH_VERIFY_SIGNATURE, &signature);
  }

  if (progress_callback) {
    progress_callback->update(0, 0);
  }
}

var::NumberString Link::get_device_result_error(s32 result) {
  const int error_number = SYSFS_GET_RETURN_ERRNO(result);
  const int return_value = SYSFS_GET_RETURN(result);
  return NumberString().format(
    "device error number: %d code %d",
    error_number,
    return_value);
}

#else
int sos_api_link_unused;
#endif
