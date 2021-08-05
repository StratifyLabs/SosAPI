// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <errno.h>
#include <sos/link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fs.hpp>
#include <printer/Printer.hpp>
#include <var.hpp>

#include "sos/Appfs.hpp"
#include "sos/Link.hpp"
#include "sos/Auth.hpp"

#if defined __link
#define FILE_BASE Link
#else
#define FILE_BASE fs
#endif

printer::Printer &
printer::operator<<(printer::Printer &printer, const appfs_file_t &a) {
  printer.key("name", var::StringView(a.hdr.name));
  printer.key("id", var::StringView(a.hdr.id));
  printer.key("mode", var::NumberString(a.hdr.mode, "0%o"));
  printer.key(
    "version",
    var::NumberString()
      .format("%d.%d", a.hdr.version >> 8, a.hdr.version & 0xff)
      .string_view());
  printer.key("startup", var::NumberString(a.exec.startup, "%p").string_view());
  printer.key(
    "codeStart",
    var::NumberString(a.exec.code_start, "%p").string_view());
  printer.key("codeSize", var::NumberString(a.exec.code_size).string_view());
  printer.key(
    "ramStart",
    var::NumberString(a.exec.ram_start, "%p").string_view());
  printer.key("ramSize", var::NumberString(a.exec.ram_size).string_view());
  printer.key("dataSize", var::NumberString(a.exec.data_size).string_view());
  printer.key(
    "oFlags",
    var::NumberString(a.exec.o_flags, "0x%lX").string_view());
  printer.key(
    "signature",
    var::NumberString(a.exec.signature, "0x%08lx").string_view());
  return printer;
}

printer::Printer &printer::operator<<(
  printer::Printer &printer,
  const sos::Appfs::Appfs::FileAttributes &a) {
  printer.key("name", a.name());
  printer.key("id", a.id());
  printer.key(
    "version",
    var::String().format("%d.%d", a.version() >> 8, a.version() & 0xff));

  printer.key("signature", var::NumberString(a.signature(), "%x"));
  printer.key_bool("flash", a.is_flash());
  printer.key_bool("codeExternal", a.is_code_external());
  printer.key_bool("dataExternal", a.is_data_external());
  printer.key_bool("codeTightlyCoupled", a.is_code_tightly_coupled());
  printer.key_bool("dataTightlyCoupled", a.is_data_tightly_coupled());
  printer.key_bool("startup", a.is_startup());
  printer.key_bool("unique", a.is_unique());
  printer.key("ramSize", var::NumberString(a.ram_size()).string_view());
  return printer;
}

printer::Printer &
printer::operator<<(printer::Printer &printer, const sos::Appfs::Info &a) {
  printer.key("name", a.name());
  printer.key("mode", var::NumberString(a.mode(), "0%o").string_view());
  if (a.is_executable()) {
    printer.key("id", a.id());
    printer.key(
      "version",
      var::String().format("%d.%d", a.version() >> 8, a.version() & 0xff));

    printer.key(
      "signature",
      var::NumberString(a.signature(), F3208X).string_view());
    printer.key("ram", var::NumberString(a.ram_size()).string_view());
    printer.key_bool("orphan", a.is_orphan());
    printer.key_bool("flash", a.is_flash());
    printer.key_bool("startup", a.is_startup());
    printer.key_bool("unique", a.is_unique());
  }
  return printer;
}

using namespace sos;

Appfs::FileAttributes::FileAttributes(const fs::FileObject &existing) {
  existing.read(View(m_file_header));
}

const Appfs::FileAttributes &
Appfs::FileAttributes::apply(const fs::FileObject &file) const {
  fs::File::LocationGuard location_guard(file);
  const size_t size = file.size();
  file.seek(0).write(var::View(m_file_header));

  API_ASSERT((file.size() == size) || (file.size() == sizeof(m_file_header)));

  return *this;
}

Appfs::Appfs(FSAPI_LINK_DECLARE_DRIVER) {
#if defined __link
  set_driver(link_driver);
#endif
}

Appfs::Appfs(const Construct &options FSAPI_LINK_DECLARE_DRIVER_LAST)
  : m_file(
    "/app/.install",
    fs::OpenMode::write_only() FSAPI_LINK_INHERIT_DRIVER_LAST) {

  FSAPI_LINK_SET_DRIVER((*this), link_driver);

  const auto path = options.mount() / "flash" / options.name();

  if (FILE_BASE::FileSystem(FSAPI_LINK_MEMBER_DRIVER).exists(path)) {
    FILE_BASE::FileSystem(FSAPI_LINK_MEMBER_DRIVER).remove(path.string_view());
  }

  if (options.is_executable() == false) {

    API_ASSERT(options.size() != 0);
    m_request = I_APPFS_CREATE;

    appfs_file_t *f
      = reinterpret_cast<appfs_file_t *>(m_create_install_attributes.buffer);

    // delete the settings if they exist
    var::View(f->hdr.name)
      .fill<u8>(0)
      .truncate(sizeof((f->hdr.name)))
      .copy(fs::Path::name(options.name()));

    f->hdr.mode = 0666;
    f->exec.code_size
      = options.size() + overhead(); // total number of bytes in file
    f->exec.signature = APPFS_CREATE_SIGNATURE;

    // f holds bytes in the buffer
    m_bytes_written = overhead();
    m_data_size = f->exec.code_size;

  } else {
    m_bytes_written = 0;
    m_data_size = 0;
    m_request = I_APPFS_INSTALL;
  }
}

Appfs &Appfs::append(
  const fs::FileObject &file,
  const api::ProgressCallback *progress_callback) {

  const auto signature = Auth::get_signature(file);
  const auto is_signature_required
    = m_file.ioctl(I_APPFS_IS_SIGNATURE_REQUIRED, nullptr).return_value() == 1;

  if (m_data_size == 0 && m_request == I_APPFS_INSTALL) {
    if (is_signature_required) {
      m_data_size = file.size() - sizeof(auth_signature_marker_t);
    } else {
      m_data_size = file.size();
    }
  }

  var::Array<char, APPFS_PAGE_SIZE> buffer;

  var::View buffer_view(buffer);
  buffer_view.fill(0);

  size_t bytes_written = 0;
  const size_t file_size = file.size();
  while (file.read(buffer_view).return_value() > 0 && bytes_written < m_data_size) {
    const auto next = bytes_written + return_value();
    const auto page_size = next > m_data_size
                             ? m_data_size - bytes_written
                             : return_value();
    bytes_written += page_size;
    append(var::View(buffer).truncate(page_size));
    if (progress_callback) {
      progress_callback->update(bytes_written, file_size);
    }
  }

  if (is_signature_required && m_request == I_APPFS_INSTALL) {
    appfs_verify_signature_t verify_signature;
    View(verify_signature.data).copy(signature.data());
    m_file.ioctl(I_APPFS_VERIFY_SIGNATURE, &verify_signature);
  }

  if (progress_callback) {
    progress_callback->update(0, 0);
  }

  return *this;
}

void Appfs::append(var::View blob) {
  u32 bytes_written = 0;
  if (m_data_size && (m_bytes_written == m_data_size)) {
    API_RETURN_ASSIGN_ERROR("", ENOSPC);
  }

  while (m_bytes_written < m_data_size && bytes_written < blob.size()) {
    const u32 page_offset = m_bytes_written % APPFS_PAGE_SIZE;
    const u32 page_size_available = APPFS_PAGE_SIZE - page_offset;
    u32 page_size = blob.size() - bytes_written;
    if (page_size > page_size_available) {
      page_size = page_size_available;
    }

    memcpy(
      m_create_install_attributes.buffer + page_offset,
      blob.to_const_u8() + bytes_written,
      page_size);

    m_bytes_written += page_size;
    bytes_written += page_size;

    if (
      ((m_bytes_written % APPFS_PAGE_SIZE) == 0) // at page boundary
      || (m_bytes_written == m_data_size)) {     // or to the end

      page_size = m_bytes_written % APPFS_PAGE_SIZE;
      if (page_size == 0) {
        m_create_install_attributes.nbyte = APPFS_PAGE_SIZE;
      } else {
        m_create_install_attributes.nbyte = page_size;
      }

      m_file.ioctl(m_request, &m_create_install_attributes);
      m_create_install_attributes.loc += m_create_install_attributes.nbyte;
    }
  }
}

bool Appfs::is_flash_available() {
  API_RETURN_VALUE_IF_ERROR(false);
  const char *first_entry
    = FILE_BASE::Dir("/app/flash" FSAPI_LINK_MEMBER_DRIVER_LAST).read();
  API_RESET_ERROR();
  return first_entry != nullptr;
}

bool Appfs::is_ram_available() {
  API_RETURN_VALUE_IF_ERROR(false);
  const char *first_entry
    = FILE_BASE::Dir("/app/ram" FSAPI_LINK_MEMBER_DRIVER_LAST).read();
  API_RESET_ERROR();
  return first_entry != nullptr;
}

var::Vector<Appfs::PublicKey> Appfs::get_public_key_list() const {
  var::Vector<Appfs::PublicKey> result;
  result.reserve(16);

  api::ErrorScope es;

  int get_key_result;
  u32 key_index = 0;
  do {
    appfs_public_key_t public_key = {.index = key_index};
    get_key_result
      = m_file.ioctl(I_APPFS_GET_PUBLIC_KEY, &public_key).return_value();
    // ensure zero terminated id
    public_key.id[sizeof(public_key.id) - 1] = 0;

    if (get_key_result == 0) {
      result.push_back(PublicKey(public_key));
    }
    key_index++;
  } while (get_key_result == 0 && key_index < 256);

  return result;
}

Appfs::Info Appfs::get_info(const var::StringView path) {
  API_RETURN_VALUE_IF_ERROR(Info());
  appfs_file_t appfs_file_header = {};
  int result = FILE_BASE::File(
                 path,
                 fs::OpenMode::read_only() FSAPI_LINK_MEMBER_DRIVER_LAST)
                 .read(var::View(appfs_file_header))
                 .return_value();

  API_RETURN_VALUE_IF_ERROR(Info());

  if (result < static_cast<int>(sizeof(appfs_file_header))) {
    API_RETURN_VALUE_ASSIGN_ERROR(Info(), "get info", ENOEXEC);
  }

  appfs_file_header.hdr.name[APPFS_NAME_MAX] = 0;

  // first check to see if the name matches -- otherwise it isn't an app
  // file
  const var::StringView path_name = fs::Path::name(path);

  if (path_name.find(".sys") == 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(Info(), "is .sys", EINVAL);
  }

  if (path_name.find(".free") == 0) {
    API_RETURN_VALUE_ASSIGN_ERROR(Info(), "is .free", EINVAL);
  }

  if ((appfs_file_header.hdr.mode & 0111) == 0) {
    // return AppfsInfo();
  }

  const var::StringView app_name = appfs_file_header.hdr.name;

  appfs_info_t info = {};
  if (
    path_name == app_name
#if defined __link
    || (path_name.find(app_name) == 0)
#endif
  ) {
    View(info.name).copy(View(appfs_file_header.hdr.name));
    info.mode = appfs_file_header.hdr.mode;
    info.version = appfs_file_header.hdr.version;
    View(info.id).copy(View(appfs_file_header.hdr.id));
    info.ram_size = appfs_file_header.exec.ram_size;
    info.o_flags = appfs_file_header.exec.o_flags;
    info.signature = appfs_file_header.exec.signature;
  } else {
    API_RETURN_VALUE_ASSIGN_ERROR(Info(), "no app name", ENOEXEC);
  }

  return Info(info);
}

#ifndef __link

Appfs &Appfs::cleanup(CleanData clean_data) {
  struct stat st;
  char buffer[LINK_PATH_MAX];
  const char *name;

  fs::Dir dir("/app/ram");

  while ((name = dir.read()) != 0) {
    strcpy(buffer, "/app/ram/");
    strcat(buffer, name);

    API_SYSTEM_CALL("", stat(buffer, &st));
    if (is_error()) {
      return *this;
    }

    if (
      ((st.st_mode & (LINK_S_IXUSR | LINK_S_IXGRP | LINK_S_IXOTH))
       || (clean_data == CleanData::yes))
      && (name[0] != '.')) {

      API_SYSTEM_CALL("", unlink(buffer));
      if (is_error()) {
        return *this;
      }
    }
  }

  return *this;
}

Appfs &Appfs::free_ram(var::StringView path) {
  fs::File(path, fs::OpenMode::read_only()).ioctl(I_APPFS_FREE_RAM, nullptr);
  return *this;
}

Appfs &Appfs::reclaim_ram(var::StringView path) {
  fs::File(path, fs::OpenMode::read_only()).ioctl(I_APPFS_RECLAIM_RAM, nullptr);
  return *this;
}

#endif
