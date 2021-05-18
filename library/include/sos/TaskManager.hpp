// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SAPI_SYS_TASK_MANAGER_HPP
#define SAPI_SYS_TASK_MANAGER_HPP

#include <sos/dev/sys.h>

#include "macros.hpp"

#include "Link.hpp"
#include "fs/File.hpp"
#include "thread/Sched.hpp"
#include "var/StringView.hpp"

namespace sos {

/*! \brief Task Class
 * \details The Task Class is used to query
 * the system resources for task attribute information.
 *
 * It is not used for creating tasks. Use sos::Thread or sos::Sys::launch()
 * to create threads or processes respectively.
 *
 * \code
 * Task task;
 * TaskAttr attr;
 * do {
 *   task.get_next(attr);
 *   if( attr.is_valid() ){
 *     if( attr.is_enabled() ){
 *       printf("Task Name is %s\n", attr.name());
 *     }
 *   }
 * } while( attr != TaskAttr::invalid() );
 * \endcode
 *
 *
 */
class TaskManager : public api::ExecutionContext {
public:
  /*!
   * \brief The Task Info Class
   */
  class Info {
  public:
    Info() : m_value{} {
      m_value.tid = static_cast<u32>(-1);
      m_value.pid = static_cast<u32>(-1);
    }

    Info(int tid) : m_value{} {
      m_value.tid = tid;
    }

    Info(const sys_taskattr_t &attr) { m_value = attr; }

    static Info invalid() { return Info(); }

    bool is_valid() const { return m_value.tid != static_cast<u32>(-1); }

    Info &set_name(const var::StringView name) {
      var::View(m_value.name)
          .fill<u8>(0)
          .truncate(sizeof(m_value.name) - 1)
          .copy(name);
      return *this;
    }

    API_AMF(Info, u32, value, pid)
    API_AMF(Info, u32, value, tid)
    API_AMF(Info, u64, value, timer)

    API_AMFWA(Info, u32, value, stack, stack_ptr)
    API_AMFWA(Info, u32, value, memory_size, mem_size)

    u32 id() const { return m_value.tid; }
    u32 thread_id() const { return m_value.tid; }
    bool is_active() const { return m_value.is_active > 0; }
    u8 priority() const { return m_value.prio; }
    u8 priority_ceiling() const { return m_value.prio_ceiling; }
    bool is_thread() const { return m_value.is_thread != 0; }
    bool is_enabled() const { return m_value.is_enabled != 0; }

    var::StringView name() const { return m_value.name; }

    u32 heap_size() const {
      if (m_value.is_thread) {
        return 0;
      }
      return m_value.malloc_loc - m_value.mem_loc;
    }

    u32 stack_size() const {
      return m_value.mem_loc + m_value.mem_size - m_value.stack_ptr;
    }

    u32 heap() const {
      if (m_value.is_thread) {
        return 0;
      }
      return m_value.mem_loc;
    }

    u8 memory_utilization() const {
      return ((heap_size() + stack_size()) * 100) / memory_size();
    }

    bool operator==(const Info &a) const {
      return (a.pid() == pid()) && (a.thread_id() == thread_id())
             && (a.name() == name());
    }

  private:
    sys_taskattr_t m_value;
  };

  using IsNull = fs::File::IsNull;

  static constexpr const char * device_path(){
    return "/dev/sys";
  }

  TaskManager() {}
  TaskManager(
    const var::StringView device FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);

  TaskManager(const TaskManager &a) = delete;
  TaskManager &operator=(const TaskManager &a) = delete;

  TaskManager(TaskManager &&a) { std::swap(m_sys_device, a.m_sys_device); }
  TaskManager &operator=(TaskManager &&a) {
    std::swap(m_sys_device, a.m_sys_device);
    return *this;
  }

  Info get_info(u32 id) const;
  var::Vector<Info> get_info();

  void print(int pid = -1);

  int get_pid(const var::StringView name);
  bool is_pid_running(pid_t pid);
  int count_total();
  int count_free();

  const TaskManager &kill_pid(int pid, int signo) const;

private:
#if defined __link
  Link::File m_sys_device;
#else
  fs::File m_sys_device;
#endif
};

} // namespace sos

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::TaskManager::Info &a);
} // namespace printer

#endif // SAPI_SYS_TASK_HPP
