/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.
#include "sos/TaskManager.hpp"
#include "printer/Printer.hpp"
#include "thread/Thread.hpp"

printer::Printer &printer::operator<<(printer::Printer &printer,
                                      const sos::TaskManager::Info &a) {
  printer.key("name", a.name());
  printer.key("id", var::NumberString(a.id()).string_view());
  printer.key("pid", var::NumberString(a.pid()).string_view());
  printer.key("memorySize", var::NumberString(a.memory_size()).string_view());
  printer.key("stack", var::NumberString(a.stack(), "0x%lX").string_view());
  printer.key("stackSize", var::NumberString(a.stack_size()).string_view());
  printer.key("priority", var::NumberString(a.priority()).string_view());
  printer.key("priorityCeiling",
              var::NumberString(a.priority_ceiling()).string_view());
  printer.key_bool("isThread", a.is_thread());
  if (a.is_thread() == false) {
    printer.key("heap", var::NumberString(a.heap(), "0x%lX").string_view());
    printer.key("heapSize", var::NumberString(a.heap_size()).string_view());
  }
  return printer;
}

using namespace sos;

TaskManager::TaskManager(IsNull is_null) : m_sys_device() {}

TaskManager::TaskManager(FSAPI_LINK_DECLARE_DRIVER)
    : m_sys_device("/dev/sys",
                   fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {}

int TaskManager::count_total() {
  int count = 0;
  while (is_success()) {
    get_info(count++);
  }
  API_RESET_ERROR();
  return count;
}

int TaskManager::count_free() {
  Info info;
  int count = 0;
  int total_count = 0;
  while (is_success()) {
    info = get_info(total_count++);
    if (!info.is_enabled()) {
      count++;
    }
  }
  API_RESET_ERROR();
  return count;
}

TaskManager::Info TaskManager::get_info(u32 id) const {
  API_RETURN_VALUE_IF_ERROR(Info());
  sys_taskattr_t attr = {0};
  attr.tid = id;
  API_SYSTEM_CALL("", m_sys_device.ioctl(I_SYS_GETTASK, &attr).return_value());
  return Info(attr);
}

var::Vector<TaskManager::Info> TaskManager::get_info() {
  int task_count = count_total();
  var::Vector<Info> result;
  result.reserve(task_count);
  int id = 0;
  while (is_success()) {
    Info info = get_info(id++);
    if (info.id() == 0) {
      info.set_name("idle");
    }
    if (info.is_enabled()) {
      result.push_back(info);
    }
  }
  API_RESET_ERROR();
  return result;
}

bool TaskManager::is_pid_running(pid_t pid) {
  Info info;
  int id = 0;
  while (is_success()) {
    info = get_info(id++);
    if ((static_cast<u32>(pid) == info.pid()) && info.is_enabled()) {
      return true;
    }
  }
  API_RESET_ERROR();
  return false;
}

int TaskManager::get_pid(const var::StringView name) {
  Info info;
  int id = 0;
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
  API_SYSTEM_CALL("",
#if defined __link
                  link_kill_pid(m_sys_device.driver(), pid, signo)
#else
                  ::kill(pid, signo)
#endif
  );
  return *this;
}
