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
  printer.key("isThread", a.is_thread());
  if (a.is_thread() == false) {
    printer.key("heap", var::NumberString(a.heap(), "0x%lX").string_view());
    printer.key("heapSize", var::NumberString(a.heap_size()).string_view());
  }
  return printer;
}

using namespace sos;

TaskManager::TaskManager(FSAPI_LINK_DECLARE_DRIVER)
    : m_sys_device("/dev/sys",
                   fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {
  m_id = 0;
}

int TaskManager::count_total() {
  TaskManager task_manager;
  Info info;
  int count = 0;
  while (task_manager.get_next(info).id() > 0) {
    count++;
  }
  return count;
}

int TaskManager::count_free() {
  TaskManager task_manager;
  Info info;
  int count = 0;
  while (task_manager.get_next(info).id() > 0) {
    if (!info.is_enabled()) {
      count++;
    }
  }
  return count;
}

TaskManager &TaskManager::get_next(Info &info) {
  API_RETURN_VALUE_IF_ERROR(*this);
  sys_taskattr_t task_attr;
  task_attr.tid = m_id;
  API_SYSTEM_CALL("",
                  m_sys_device.ioctl(I_SYS_GETTASK, &task_attr).return_value());

  if (is_error()) {
    // it is normal for an error when the id exceeds the number available
    API_RESET_ERROR();
    m_id = 0;
    info = Info::invalid();
  } else {
    m_id++;
    info = task_attr;
  }

  return *this;
}

TaskManager::Info TaskManager::get_info(u32 id) const {
  API_RETURN_VALUE_IF_ERROR(Info());
  sys_taskattr_t attr;
  attr.tid = id;
  if (API_SYSTEM_CALL(
          "", m_sys_device.ioctl(I_SYS_GETTASK, &attr).return_value()) < 0) {
    return Info::invalid();
  }

  return Info(attr);
}

bool TaskManager::is_pid_running(pid_t pid) {
  TaskManager task_manager;
  Info info;
  while (task_manager.get_next(info).id() > 0) {
    if ((static_cast<u32>(pid) == info.pid()) && info.is_enabled()) {
      return true;
    }
  }
  return false;
}

int TaskManager::get_pid(const var::StringView name) {
  TaskManager task_manager;
  Info info;
  while (task_manager.get_next(info).id() > 0) {
    if (name == info.name() && info.is_enabled()) {
      return info.pid();
    }
  }
  return -1;
}
