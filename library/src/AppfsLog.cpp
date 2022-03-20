// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <fs/ViewFile.hpp>
#include <printer/Printer.hpp>

#include "sos/AppfsLog.hpp"

using namespace sos;

#if !defined __link
AppfsLog::AppfsLog(const Construct &options FSAPI_LINK_DECLARE_DRIVER_LAST)
  : m_entry_size(options.entry_size), m_maximum_size(options.maximum_size),
    m_fill_size(get_effective_entry_size() - m_entry_size - sizeof(Header)),
    m_appfs(Appfs::Construct()
              .set_size(options.maximum_size)
              .set_name(options.name)
              .set_executable(false)
              .set_overwrite(options.is_overwrite)
                FSAPI_LINK_INHERIT_DRIVER_LAST) {
  // if this is new -- fill the first page
  if (m_appfs.bytes_written() < Appfs::page_size()) {
    append_aligned(header_size);
  }

  m_read_file = fs::File(var::PathString("/app/flash") / options.name);
}

AppfsLog &AppfsLog::save_entry(var::View entry) {
  API_ASSERT(entry.size() == m_entry_size);
  m_appfs.append(fs::ViewFile(var::View(start_value)))
    .append(fs::ViewFile(entry));
  append_aligned(m_fill_size);
  return *this;
}

AppfsLog &AppfsLog::read(size_t entry_offset, var::View entry) {
  API_ASSERT(entry.size() == m_entry_size);
  m_read_file
    .seek(header_size + entry_offset * get_effective_entry_size() + sizeof(u32))
    .read(entry);
  return *this;
}

size_t AppfsLog::get_entry_count() const {
  const auto effective_size = get_effective_entry_size();
  const auto maximum_count = get_maximum_entry_count();
  for (auto count : api::Index(maximum_count)) {
    u32 start;
    m_read_file.seek(header_size + count * effective_size)
      .read(var::View(start));
    if (is_error()) {
      printer::Printer().object("error", error());
    }
    if (start != start_value) {
      return count;
    }
  }
  return maximum_count;
}

size_t AppfsLog::get_maximum_entry_count() const {
  return m_appfs.size() / get_effective_entry_size();
}

size_t AppfsLog::get_effective_entry_size() const {
  return ((m_entry_size + sizeof(Header) + Appfs::page_size() - 1)
          / Appfs::page_size())
         * Appfs::page_size();
}

AppfsLog &AppfsLog::read_newest(var::View entry) {
  const auto count = get_entry_count();
  if (count) {
    return read(count - 1, entry);
  }
  API_RETURN_VALUE_ASSIGN_ERROR(*this, "log is empty", ENOENT);
}

void AppfsLog::append_aligned(size_t size) {
  u8 buffer[size];
  var::View buffer_view(buffer, size);
  m_appfs.append(fs::ViewFile(buffer_view.fill(0)));
}

#endif
