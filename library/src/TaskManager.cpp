/*! \file */ // Copyright 2011-2020 Tyler Gilbert and Stratify Labs, Inc; see
             // LICENSE.md for rights.
#include "sos/TaskManager.hpp"
#include "printer/Printer.hpp"
#include "thread/Thread.hpp"

printer::Printer &printer::operator<<(printer::Printer &printer,
                                      const sos::TaskInfo &a) {
  printer.key("name", a.name());
  printer.key("id", var::NumberString(a.id()));
  printer.key("pid", var::NumberString(a.pid()));
  printer.key("memorySize", var::NumberString(a.memory_size()));
  printer.key("stack", var::NumberString(a.stack(), "0x%lX"));
  printer.key("stackSize", var::NumberString(a.stack_size()));
  printer.key("priority", var::NumberString(a.priority()));
  printer.key("priorityCeiling", var::NumberString(a.priority_ceiling()));
  printer.key("isThread", a.is_thread());
  if (a.is_thread() == false) {
    printer.key("heap", var::NumberString(a.heap(), "0x%lX"));
    printer.key("heapSize", var::NumberString(a.heap_size()));
  }
  return printer;
}

using namespace sos;

TaskManager::TaskManager(FSAPI_LINK_DECLARE_DRIVER)
  : m_sys_device(
    "/dev/sys",
    fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {
  m_id = 0;
}

int TaskManager::count_total() {
  int idx = m_id;
  int count = 0;
  set_id(0);
  TaskInfo attr;
  while (get_next(attr) >= 0) {
    count++;
  }
  set_id(idx);
  return count;
}

int TaskManager::count_free() {
  int idx = m_id;
  int count = 0;
  set_id(0);
  TaskInfo attr;
  while (get_next(attr) >= 0) {
    if (!attr.is_enabled()) {
      count++;
    }
  }
  set_id(idx);
  return count;
}

int TaskManager::get_next(TaskInfo &info) {
  sys_taskattr_t task_attr;
  int ret;
  task_attr.tid = m_id;
  API_SYSTEM_CALL(
    "",
    m_sys_device.ioctl(I_SYS_GETTASK, &task_attr).return_value());

  if (is_error()) {
    info = TaskInfo::invalid();
  } else {
    info = task_attr;
  }

  m_id++;
  return ret;
}

#if !defined __link
TaskInfo TaskManager::get_info() {
  TaskManager manager;
  return manager.get_info(thread::Thread::self());
}
#endif

TaskInfo TaskManager::get_info(u32 id) {
  sys_taskattr_t attr;
  attr.tid = id;
  initialize();
  if (
    API_SYSTEM_CALL(
      "",
      m_sys_device.ioctl(I_SYS_GETTASK, &attr).return_value())
    < 0) {
    return TaskInfo::invalid();
  }

  return TaskInfo(attr);
}

bool TaskManager::is_pid_running(pid_t pid) {
  int tmp_id = id();
  set_id(1);

  TaskInfo info;
  while (get_next(info) > 0) {
    if ((static_cast<u32>(pid) == info.pid()) && info.is_enabled()) {
      set_id(tmp_id);
      return true;
    }
  }

  set_id(tmp_id);
  return false;
}

int TaskManager::get_pid(const var::String &name) {
  int tmp_id = id();
  set_id(1);

  TaskInfo info;

  while (get_next(info) > 0) {
    if (name == info.name() && info.is_enabled()) {
      set_id(tmp_id);
      return info.pid();
    }
  }

  set_id(tmp_id);
  return -1;
}
