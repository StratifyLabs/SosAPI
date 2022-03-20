//
// Created by Tyler Gilbert on 3/18/22.
//

#if defined __link

#include "sos/Link.hpp"

using namespace sos;
using namespace fs;

Link::File::File(
  var::StringView name,
  fs::OpenMode flags,
  link_transport_mdriver_t *driver)
  : m_file_resource(
    open(name, flags, fs::Permissions(0666), driver),
    &file_deleter) {}

Link::File::File(
  IsOverwrite is_overwrite,
  var::StringView path,
  fs::OpenMode open_mode,
  fs::Permissions perms,
  link_transport_mdriver_t *driver)
  : m_file_resource(
    create(is_overwrite, path, open_mode, perms, driver),
    &file_deleter) {}

int Link::File::fileno() const { return fd(); }

const Link::File &Link::File::sync() const {
  API_RETURN_VALUE_IF_ERROR(*this);
#if defined __link
  if (driver()) {
    return *this;
  }
#endif
  if (fd() >= 0) {
#if !defined __win32
    API_SYSTEM_CALL("", internal_fsync(fd()));
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
  return API_SYSTEM_CALL("", link_fstat(driver(), fd(), st));
}

int Link::File::interface_read(void *buf, int nbyte) const {
  return link_read(driver(), fd(), buf, nbyte);
}

int Link::File::interface_write(const void *buf, int nbyte) const {
  return link_write(driver(), fd(), buf, nbyte);
}

int Link::File::interface_ioctl(int request, void *argument) const {
  return link_ioctl(driver(), fd(), request, argument);
}

int Link::File::internal_fsync(int fd) const {
#if defined __link
  MCU_UNUSED_ARGUMENT(fd);
  return 0;
#else
  return ::fsync(fd);
#endif
}

int Link::File::interface_lseek(int offset, int whence) const {
  return link_lseek(driver(), fd(), offset, whence);
}

Link::File::FileResource Link::File::open(
  var::StringView path,
  OpenMode flags,
  Permissions permissions,
  link_transport_mdriver_t *driver) {
  API_RETURN_VALUE_IF_ERROR({});
  const var::PathString path_string(path);
  FileResource result = {.driver = driver};
  API_SYSTEM_CALL(
    path_string.cstring(),
    result.fd = link_open(
      driver,
      path_string.cstring(),
      static_cast<int>(flags.o_flags()),
      permissions.permissions()));
  return result;
}

Link::File::FileResource Link::File::create(
  IsOverwrite is_overwrite,
  var::StringView path,
  OpenMode open_mode,
  Permissions perms,
  link_transport_mdriver_t *driver) {
  OpenMode flags = OpenMode(open_mode).set_create();
  if (is_overwrite == IsOverwrite::yes) {
    flags.set_truncate();
  } else {
    flags.set_exclusive();
  }

  return open(path, flags, perms, driver);
}

void Link::File::file_deleter(Link::File::FileResource *file_resource) {
  if (file_resource->fd >= 0) {
    link_close(file_resource->driver, file_resource->fd);
    file_resource->fd = -1;
  }
}

#endif