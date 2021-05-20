#ifndef SHARE_GC_Z_ZMEMORYBANDWIDTHMONITOR_HPP
#define SHARE_GC_Z_ZMEMORYBANDWIDTHMONITOR_HPP

#include "gc/shared/concurrentGCThread.hpp"

class ZMemoryBandwidthMonitor : public ConcurrentGCThread {
private:
  static ZMemoryBandwidthMonitor* _monitor;

  bool _enabled;
  void crash();

  // Intel Resource Director Technology
  enum Rdt_result {
    RDT_RES_OK,
    RDT_RES_NOT_SUPPORTED
  };
  Rdt_result cpu_support_rdt();

  int resctrl_fd;
  bool resctrl_mount();

  void resctrl_get_read_lock();
  void resctrl_get_write_lock();
  void resctrl_release_lock();
  FILE* resctrl_open(const char* path, const char* flags);
  bool resctrl_check_status();
  void resctrl_assign_cos(int group, pid_t pid);
  long resctrl_read_mbm(int group);

protected:
  virtual void run_service();
  virtual void stop_service();

public:
  static ZMemoryBandwidthMonitor* monitor();
  ZMemoryBandwidthMonitor();

  bool is_enabled();
  const char* to_string();  
  void add_thread(pid_t tid);
};

#endif