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
#include <fs.hpp>
#include <var.hpp>

#include "sos/Appfs.hpp"
#include "sos/Link.hpp"

#define MAX_TRIES 3

using namespace fs;
using namespace sos;

static var::String gen_error(const var::String &msg, int err_number) {
  var::String s;
  s.format("%s (%d)", msg.cstring(), err_number);
  return s;
}

Link::Link() { link_load_default_driver(driver()); }

Link::~Link() {}

Link &Link::reset_progress() {
  m_progress = 0;
  m_progress_max = 0;
  return *this;
}

fs::PathList Link::get_path_list() {
  fs::PathList result;
  PathString device_name;
  PathString last_device;

  while (driver()->getname(device_name.to_char(), last_device.cstring(),
                           static_cast<int>(device_name.capacity())) == 0) {

    result.push_back(device_name); // this will make a copy of device name and
                                   // put it on the list
    last_device = device_name;
  }

  return std::move(result);
}

var::Vector<Link::Info> Link::get_info_list() {
  var::Vector<Info> result;
  fs::PathList port_list = get_path_list();

  // disconnect if already connected
  disconnect();

  for (u32 i = 0; i < port_list.count(); i++) {
    // ping and grab the info
    connect(port_list.at(i));
    // couldn't connect
    if (is_success()) {
      result.push_back(Info(port_list.at(i), sys_info()));
      disconnect();
    } else {
      API_RESET_ERROR();
    }
  }

  return result;
}

Link &Link::connect(var::StringView path, IsLegacy is_legacy) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;

  reset_progress();

  if (driver()->phy_driver.handle == LINK_PHY_OPEN_ERROR) {

    driver()->transport_version = 0;
    const var::PathString path_string(path);

    driver()->phy_driver.handle = API_SYSTEM_CALL_NULL(
        path_string.cstring(),
        driver()->phy_driver.open(path_string.cstring(), driver()->options));

    API_RETURN_VALUE_IF_ERROR(*this);
  }

  m_is_legacy = is_legacy;

  if (m_is_legacy == IsLegacy::yes) {
    err = API_SYSTEM_CALL("", link_isbootloader_legacy(driver()));
  } else {
    err = API_SYSTEM_CALL("", link_isbootloader(driver()));
  }

  if (err > 0) {
    m_is_bootloader = IsBootloader::yes;
  } else if (err == 0) {
    m_is_bootloader = IsBootloader::no;
  } else {

    driver()->phy_driver.close(&driver()->phy_driver.handle);
    return *this;
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

  m_link_info.set_port(path).set_info(Sys::Info(sys_info));

  return *this;
}

Link &Link::reconnect(int retries, chrono::MicroTime delay) {
  API_RETURN_VALUE_IF_ERROR(*this);
  Info last_info(info());
  for (u32 i = 0; i < retries; i++) {
    connect(last_info.port());

    if (is_success()) {
      if (last_info.serial_number() == info().serial_number()) {
        return *this;
      }
      disconnect();
    }

    fs::PathList port_list = get_path_list();
    for (u32 j = 0; j < port_list.count(); j++) {
      connect(port_list.at(j));

      if (is_success()) {
        if (last_info.serial_number() == info().serial_number()) {
          return *this;
        }
        disconnect();
      }
    }

    delay.wait();
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
  if (err < 0) {
    m_error_message.format("Failed to read flash", link_errno);
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
  if (err < 0) {
    m_error_message.format("Failed to write flash", link_errno);
  }
  return *this;
}

Link &Link::disconnect() {
  API_RETURN_VALUE_IF_ERROR(*this);

  if (driver()->phy_driver.handle != LINK_PHY_OPEN_ERROR) {
    link_disconnect(driver());

    if (driver()->phy_driver.handle != LINK_PHY_OPEN_ERROR) {
      // can't unlock the device if it has been destroyed
    }
  }

  m_is_bootloader = IsBootloader::no;
  m_error_message = "";
  m_stdout_fd = -1;
  m_stdin_fd = -1;
  return *this;
}

Link &Link::set_disconnected() {
  API_RETURN_VALUE_IF_ERROR(*this);
  driver()->transport_version = 0;
  driver()->phy_driver.handle = LINK_PHY_OPEN_ERROR;
  return *this;
}

bool Link::is_connected() const {
  API_RETURN_VALUE_IF_ERROR(false);
  if (driver()->phy_driver.handle == LINK_PHY_OPEN_ERROR) {
    return false;
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
    m_error_message.format("can't set time for bootloader");
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

Link &Link::format(const var::String &path) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  if (is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }
  m_error_message = "";
  // Format the filesystem

  for (int tries = 0; tries < MAX_TRIES; tries++) {
    err = link_mkfs(driver(), path.cstring());
    if (err != LINK_PROT_ERROR)
      break;
  }

  if (err < 0) {
    m_error_message.format(
      "failed to format filesystem with device errno %d",
      link_errno);
  }
  return *this;
}

Link &Link::reset_bootloader() {
  API_RETURN_VALUE_IF_ERROR(*this);

  link_resetbootloader(driver());

  driver()->transport_version = 0;
  driver()->phy_driver.handle = LINK_PHY_OPEN_ERROR;
  return *this;
}

Link &Link::get_bootloader_attr(bootloader_attr_t &attr) {
  API_RETURN_VALUE_IF_ERROR(*this);
  int err = -1;
  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  if (is_legacy()) {
    err = link_bootloader_attr_legacy(driver(), &attr, 0);
  } else {
    err = link_bootloader_attr(driver(), &attr, 0);
  }

  return *this;
}

u32 Link::validate_os_image_id_with_connected_bootloader(
    const File *source_image) {
  API_RETURN_VALUE_IF_ERROR(0);
  int err = -1;
  u32 image_id;

  if (!is_bootloader()) {
    m_error_message = "Target is not a bootloader";
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
  int err;

  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  const api::ProgressCallback *progress_callback =
      options.printer()->progress_callback();

  options.printer()->progress_key() = "erasing";

  if (progress_callback) {
    progress_callback->update(
      0,
      api::ProgressCallback::indeterminate_progress_total());
  }
  // first erase the flash
  err = link_eraseflash(driver());

  if (err < 0) {
    if (progress_callback) {
      progress_callback->update(0, 0);
    }
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  bootloader_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  int retry = 0;
  do {
    chrono::wait(500_milliseconds);
    get_bootloader_attr(attr);

    if (progress_callback) {
      progress_callback->update(
        retry,
        api::ProgressCallback::indeterminate_progress_total());
    }
  } while ((err < 0) && (retry++ < options.bootloader_retry_count()));

  chrono::wait(250_milliseconds);

  // flush just incase the protocol gets filled with get attr requests
  driver()->phy_driver.flush(driver()->phy_driver.handle);

  if (progress_callback) {
    progress_callback->update(0, 0);
  }

  if (err < 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  return *this;
}

Link &Link::install_os(u32 image_id, const UpdateOs &options) {
  API_RETURN_VALUE_IF_ERROR(*this);

  // must be connected to the bootloader with an erased OS
  int err = -1;
  const int buffer_size = 1024;

  const api::ProgressCallback *progress_callback
    = options.printer()->progress_callback();

  var::Data start_address_buffer(256);
  var::Data buffer(buffer_size);
  var::Data compare_buffer(buffer_size);

  if (!is_bootloader()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EIO);
  }

  if (options.image()->seek(0).is_error()) {
    API_RETURN_VALUE_ASSIGN_ERROR(*this, "", EINVAL);
  }

  u32 start_address = m_bootloader_attributes.startaddr;
  u32 loc = start_address;

  options.printer()->progress_key() = "installing";

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
      var::View buffer_view =
          var::View(buffer).truncate(start_address_buffer.size());
      start_address_buffer.copy(buffer_view);

      // memcpy(stackaddr, buffer, 256);

      buffer_view.fill<u8>(0xff);
    }

    if (
      (err = link_writeflash(driver(), loc, buffer.data(), bytes_read))
      != bytes_read) {
      m_error_message.format(
        "Failed to write to link flash at 0x%x (%d, %d) -> try the operation "
        "again",
        loc,
        err,
        link_errno);
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
          m_error_message.format("Failed to read flash memory", link_errno);
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
  API_RETURN_VALUE_IF_ERROR(*this);

  API_ASSERT(options.image() != nullptr);
  API_ASSERT(options.printer() != nullptr);

  u32 image_id
    = validate_os_image_id_with_connected_bootloader(options.image());

  if (image_id == 0) {
    return *this;
  }

  var::String progress_key = var::String(options.printer()->progress_key());

  erase_os(options);
  install_os(image_id, options);

  options.printer()->progress_key() = progress_key;
  return *this;
}

var::String Link::DriverPath::lookup_serial_port_path_from_usb_details() {

#if defined __win32

#endif
  return var::String();
}

#else
int sos_api_link_unused;
#endif
