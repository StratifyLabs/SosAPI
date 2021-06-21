// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include "sos/TaskManager.hpp"
#include "printer/Printer.hpp"
#include "thread/Thread.hpp"

printer::Printer &printer::operator<<(
  printer::Printer &printer,
  const sos::TaskManager::Info &a) {
  printer.key("name", a.name());
  printer.key("id", var::NumberString(a.id()).string_view());
  printer.key("pid", var::NumberString(a.pid()).string_view());
  printer.key("memorySize", var::NumberString(a.memory_size()).string_view());
  printer.key("stack", var::NumberString(a.stack(), "0x%lX").string_view());
  printer.key("stackSize", var::NumberString(a.stack_size()).string_view());
  printer.key("priority", var::NumberString(a.priority()).string_view());
  printer.key(
    "priorityCeiling",
    var::NumberString(a.priority_ceiling()).string_view());
  printer.key_bool("isThread", a.is_thread());
  if (a.is_thread() == false) {
    printer.key("heap", var::NumberString(a.heap(), "0x%lX").string_view());
    printer.key("heapSize", var::NumberString(a.heap_size()).string_view());
  }
  return printer;
}

using namespace sos;

TaskManager::TaskManager(
  const var::StringView device_path FSAPI_LINK_DECLARE_DRIVER_LAST)
  : m_sys_device(
    device_path.is_empty() ? var::StringView("/dev/sys") : device_path,
    fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {}

int TaskManager::count_total() {
  int count = 0;
  api::ErrorScope error_scope;
  while (is_success()) {
    get_info(count++);
  }
  return count;
}

int TaskManager::count_free() {
  Info info;
  int count = 0;
  int total_count = 0;
  api::ErrorScope error_scope;
  while (is_success()) {
    info = get_info(total_count++);
    if (!info.is_enabled()) {
      count++;
    }
  }
  return count;
}

TaskManager::Info TaskManager::get_info(u32 id) const {
  API_RETURN_VALUE_IF_ERROR(Info());
  sys_taskattr_t attr{};
  attr.tid = id;
  m_sys_device.ioctl(I_SYS_GETTASK, &attr);
  return Info(attr);
}

var::Vector<TaskManager::Info> TaskManager::get_info() {
  var::Vector<Info> result;
  result.reserve(64);
  int id = 0;
  api::ErrorScope error_scope;
  while (is_success()) {
    Info info = get_info(id++);
    if (is_success()) {
      if (info.id() == 0) {
        info.set_name("idle");
      }
      if (info.is_enabled()) {
        result.push_back(info);
      }
    }
  }

  return result;
}

bool TaskManager::is_pid_running(pid_t pid) {
  Info info;
  int id = 0;
  api::ErrorScope error_scope;
  while (is_success()) {
    info = get_info(id++);
    if ((static_cast<u32>(pid) == info.pid()) && info.is_enabled()) {
      return true;
    }
  }
  return false;
}

int TaskManager::get_pid(const var::StringView name) {
  Info info;
  int id = 0;
  api::ErrorScope error_scope;
  while (is_success()) {
    info = get_info(id++);
    if (name == info.name() && info.is_enabled()) {
      return info.pid();
    }
  }
  return -1;
}

const TaskManager &TaskManager::kill_pid(int pid, int signo) const {
  API_RETURN_VALUE_IF_ERROR(*this);
  API_SYSTEM_CALL(
    "",
#if defined __link
    link_kill_pid(m_sys_device.driver(), pid, signo)
#else
    ::kill(pid, signo)
#endif
  );
  return *this;
}
