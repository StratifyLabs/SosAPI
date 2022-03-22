//
// Created by Tyler Gilbert on 3/18/22.
//
#if defined __link

#include "sos/Link.hpp"

using namespace sos;

Link::Dir::Dir(var::StringView path, link_transport_mdriver_t *driver)
  : m_directory_system_resource(open(path, driver), &directory_deleter) {}

Link::Dir::DirectoryResource
Link::Dir::open(var::StringView path, link_transport_mdriver_t *driver) {
  API_RETURN_VALUE_IF_ERROR({});
  DirectoryResource result{};
  result.driver = driver;
  result.path = path;
  result.dirp = API_SYSTEM_CALL_NULL(
    result.path.cstring(),
    reinterpret_cast<DIR *>(
      link_opendir(result.driver, result.path.cstring())));
  if (result.dirp == nullptr) {
    result.path = {};
  }
  return result;
}

void Link::Dir::directory_deleter(
  Link::Dir::DirectoryResource *directory_resource) {
  API_RETURN_IF_ERROR();
  if (directory_resource->dirp) {
    API_SYSTEM_CALL(
      directory_resource->path.cstring(),
      link_closedir(directory_resource->driver, directory_resource->dirp));
    directory_resource->dirp = nullptr;
  }

  directory_resource->path.clear();
}

int Link::Dir::interface_readdir_r(
  struct dirent *result,
  struct dirent **resultp) const {
  return link_readdir_r(driver(), dirp(), result, resultp);
}

long Link::Dir::interface_telldir() const {
  return link_telldir(driver(), dirp());
}

void Link::Dir::interface_seekdir(size_t location) const {
  link_seekdir(driver(), dirp(), location);
}

void Link::Dir::interface_rewinddir() const {
  link_rewinddir(driver(), dirp());
}

#endif
