// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOS_API_SOS_APPFS_LOG_HPP_
#define SOS_API_SOS_APPFS_LOG_HPP_

#include "Appfs.hpp"

namespace sos {

#if !defined __link
class AppfsLog : public api::ExecutionContext {
public:
  struct Construct {
    API_PMAZ(entry_size, Construct, size_t, {});
    API_PUBLIC_BOOL(Construct, overwrite, true);
    API_PMAZ(maximum_size, Construct, size_t, 4096);
    API_PMAZ(name, Construct, var::StringView, {});
  };

  explicit AppfsLog(
    const Construct &options FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);

  AppfsLog &save_entry(var::View entry);
  AppfsLog &read(size_t entry_offset, var::View entry);
  AppfsLog &read_newest(var::View entry);

  size_t get_entry_count() const;
  size_t get_maximum_entry_count() const;
  size_t get_effective_entry_size() const;

private:
  struct Header {
    u32 start;
  };

  static constexpr u32 start_value = 0x11223344;
  static constexpr size_t header_size = Appfs::page_size() - Appfs::overhead();

  size_t m_entry_size = 0;
  size_t m_maximum_size = 0;
  size_t m_fill_size = 0;
  Appfs m_appfs;
  fs::File m_read_file;

  void append_aligned(size_t size);
};

#endif

} // namespace sos


#endif /* SOS_API_SOS_APPFS_LOG_HPP_ */
