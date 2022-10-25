//
// Created by Tyler Gilbert on 3/18/22.
//

#if defined __link

#include <fs/Dir.hpp>
#include <fs/FileSystem.hpp>
#include <fs/Path.hpp>

#include "sos/Link.hpp"


using namespace sos;
using namespace fs;


Link::FileSystem::FileSystem(link_transport_mdriver_t *driver) {
  set_driver(driver);
}

const Link::FileSystem &
Link::FileSystem::remove(const var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  const var::PathString path_string(path);
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_unlink(path_string.cstring()));
  return *this;
}

const Link::FileSystem &
Link::FileSystem::touch(const var::StringView path) const {
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

bool Link::FileSystem::exists(const var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(false);
  std::ignore = get_info(path);
  const auto result = is_success();
  API_RESET_ERROR();
  return result;
}

fs::FileInfo Link::FileSystem::get_info(const var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(FileInfo());
  const var::PathString path_string(path);
  struct stat stat = {};
  API_SYSTEM_CALL(
    path_string.cstring(),
    interface_stat(path_string.cstring(), &stat));
  return FileInfo(stat);
}

fs::FileInfo Link::FileSystem::get_info(const File &file) const {
  API_RETURN_VALUE_IF_ERROR(FileInfo());
  struct stat stat = {};
  API_SYSTEM_CALL("", interface_fstat(file.fileno(), &stat));
  return FileInfo(stat);
}

const Link::FileSystem &Link::FileSystem::remove_directory(
  const var::StringView path,
  IsRecursive recursive) const {

  if (recursive == IsRecursive::yes) {
    Dir d(path, driver());

    var::String entry;
    while ((entry = d.read()).is_empty() == false) {
      var::PathString entry_path = path / entry;
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
Link::FileSystem::remove_directory(const var::StringView path) const {
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
  bool (*exclude)(var::StringView entry, void *context),
  void *context) const {
  PathList result;
  bool is_the_end = false;

  Dir directory(path, driver());

  do {
    const char *entry_result = directory.read();
    const var::PathString entry = (entry_result != nullptr)
                                    ? var::PathString(entry_result)
                                    : var::PathString();

    if (entry.is_empty()) {
      is_the_end = true;
    }

    if (
      (exclude == nullptr || !exclude(entry.string_view(), context))
      && !entry.is_empty() && (entry.string_view() != ".")
      && (entry.string_view() != "..")) {

      if (is_recursive == IsRecursive::yes) {

        const var::PathString entry_path
          = var::PathString(directory.path()) / entry.string_view();
        FileInfo info = get_info(entry_path.cstring());

        if (info.is_directory()) {
          PathList intermediate_result
            = read_directory(entry_path, is_recursive, exclude, context);

          for (const auto &intermediate_entry : intermediate_result) {
            result.push_back(entry / intermediate_entry);
          }
        } else {
          result.push_back(entry);
        }
      } else {
        result.push_back(entry);
      }
    }
  } while (!is_the_end);

  return result;
}

bool Link::FileSystem::directory_exists(const var::StringView path) const {
  API_RETURN_VALUE_IF_ERROR(false);
  FileInfo info = get_info(path);
  const bool result = is_success() && info.is_directory();
  API_RESET_ERROR();
  return result;
}

fs::Permissions
Link::FileSystem::get_permissions(const var::StringView path) const {
  const var::StringView parent = fs::Path::parent_directory(path);
  if (parent.is_empty()) {
    return get_info(".").permissions();
  }

  return get_info(parent).permissions();
}

const Link::FileSystem &Link::FileSystem::create_directory(
  const var::StringView path,
  const Permissions &permissions) const {

  if (directory_exists(path)) {
    return *this;
  }

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
    = var::Tokenizer(path, var::Tokenizer::Construct().set_delimiters("/"));
  var::String base_path;

  // tokenizer will strip the first / and create an empty token
  if (path.length() && path.front() == '/') {
    base_path += "/";
  }

  for (u32 i = 0; i < path_tokens.count(); i++) {
    if (path_tokens.at(i).is_empty() == false) {
      base_path += path_tokens.at(i);
      if (create_directory(base_path, permissions).is_error()) {
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

#endif