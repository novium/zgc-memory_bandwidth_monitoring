#include "runtime/java.hpp"
#include "gc/z/zCollectedHeap.hpp"
#include "gc/z/zMemoryBandwidthMonitor.hpp"
#include "gc/z/zHeap.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zWorkers.hpp"
#include "logging/log.hpp"

#include <sys/file.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdlib.h>

ZMemoryBandwidthMonitor* ZMemoryBandwidthMonitor::_monitor = NULL;

class ZMemoryBandwidthMonitorThreadClosure : public ThreadClosure {
public:
  virtual void do_thread(Thread* thread) {
    ZMemoryBandwidthMonitor::monitor()->add_thread(thread->osthread()->thread_id());
  }
};

ZMemoryBandwidthMonitor::ZMemoryBandwidthMonitor() {
  _enabled = ZUseMemoryBandwidthMonitor;
  _monitor = this;

  if(_enabled) {
    log_info(gc, zbw)("➡️  Starting memory bandwidth monitor");
    set_name("ZMemoryBandwidthMonitor");
    create_and_start();
  }
}

ZMemoryBandwidthMonitor* ZMemoryBandwidthMonitor::monitor() {
  assert(_monitor != NULL, "Not initialized");
  return _monitor;
}

const char* ZMemoryBandwidthMonitor::to_string() {
  return _enabled ? "Enabled" : "Disabled";
}

void ZMemoryBandwidthMonitor::run_service() {
  if(cpu_support_rdt() != RDT_RES_OK 
  || !resctrl_mount()) {
    crash();
  }

  ZMemoryBandwidthMonitorThreadClosure cl;
  ZCollectedHeap::heap()->gc_threads_do(&cl);

  long total = 0;
  long prev  = 0;
  while(true) {
    if(this->should_terminate()) {
      break;
    }

    long total = resctrl_read_mbm(2);

    if(total > prev) {
        log_info(gc,zbw,stats)("%ld", total - prev);
    } else {
        log_info(gc,zbw,stats)("%ld", (__UINT64_MAX__ - prev) + total);
    }

    prev = total;

    sleep(1);
  }
}

void ZMemoryBandwidthMonitor::stop_service() {
  log_info(gc, zbw)("➡️  Stopping memory bandwidth monitor");
}

void ZMemoryBandwidthMonitor::crash() {
  log_error(gc, zbw)("☠️  Memory bandwidth monitor crashed, aborting...");
  vm_abort(false);
}

ZMemoryBandwidthMonitor::Rdt_result ZMemoryBandwidthMonitor::cpu_support_rdt() {
  log_debug(gc,zbw)("➡️  Checking Intel Resource Director Technology support (Intel RDT)");

  Rdt_result result = RDT_RES_OK;

  // Registers
  unsigned int __eax, __ebx, __ecx, __edx;

  // Get and check vendor id
  __asm__ volatile (
    "mov $0x0, %%eax\n\t"
    "cpuid\n\t"
	  : "=&a" (__eax), "=&b" (__ebx), "=&c" (__ecx), "=&d" (__edx));
  
  char vendor_id[13];

  for(int i = 0; i < 4; i++) {
    vendor_id[0 + i] = ((const char *) &__ebx)[i];
  }
  for(int i = 0; i < 4; i++) {
    vendor_id[4 + i] = ((const char *) &__edx)[i];
  }
  for(int i = 0; i < 4; i++) {
    vendor_id[8 + i] = ((const char *) &__ecx)[i];
  }
  vendor_id[12] = '\0';

  log_debug(gc,zbw)("%s  CPU Vendor\t: %s", strcmp(vendor_id, "GenuineIntel") == 0 ? "✔️" : "☠️", vendor_id);

  if(strcmp(vendor_id, "GenuineIntel") != 0)
    result = RDT_RES_NOT_SUPPORTED;


  // Check CPU family+model+stepping
  __asm__ volatile (
    "mov $0x1, %%eax\n\t"
    "cpuid\n\t"
	  : "=&a" (__eax), "=&b" (__ebx), "=&c" (__ecx), "=&d" (__edx));

    uint32_t family   = (__eax >> 8) & 0xf;
    uint32_t model    = (__eax >> 4) & 0xf;
    uint32_t stepping = (__eax >> 0) & 0xf;

    uint32_t efamily   = (__eax >> 20) & 0xff;
    uint32_t emodel    = ((__eax >> 16) & 0xf) << 4;

  log_debug(gc,zbw)("❔️ CPU Ident\t: %02x_%02xH-%#01x", family+efamily, model+emodel, stepping);

  // TODO: Check support

  if(result == RDT_RES_OK) {
    log_debug(gc, zbw)("✔️  CPU RDT is supported!");
  } else {
    log_warning(gc, zbw)("❌️  CPU RDT is NOT supported!");
  }
  
  return result;
}

bool ZMemoryBandwidthMonitor::resctrl_mount() {
  log_info(gc, zbw)("➡️  Mounting resctrl filesystem");

  resctrl_fd = open("/sys/fs/resctrl", O_DIRECTORY);
  if(resctrl_fd == -1) {
    log_error(gc, zbw)("❌️  Failed to mount resctrl filesystem, reason: ");
    perror("open");
    return false;
  }

  log_debug(gc, zbw)("✔️  Successfully mounted resctrl filesystem");
  return true;
}

void ZMemoryBandwidthMonitor::resctrl_get_read_lock() {
  int ret;
  
  ret = flock(resctrl_fd, LOCK_SH);
  if(ret) {
    log_error(gc, zbw)("❌️  Failed to get resctrl read lock, reason: ");
    perror("flock");
    crash();
  }
}

void ZMemoryBandwidthMonitor::resctrl_get_write_lock() {
  int ret;
  
  ret = flock(resctrl_fd, LOCK_EX);
  if(ret) {
    log_error(gc, zbw)("❌️  Failed to get resctrl write lock, reason: ");
    perror("flock");
    crash();
  }
}

void ZMemoryBandwidthMonitor::resctrl_release_lock() {
  int ret;
  
  ret = flock(resctrl_fd, LOCK_SH);
  if(ret) {
    log_error(gc, zbw)("❌️  Failed to release resctrl lock, reason: ");
    perror("flock");
    crash();
  }
}

FILE* ZMemoryBandwidthMonitor::resctrl_open(const char* path, const char* flags) {
  FILE* fd;

  fd = fopen(path, flags);

  if(fd == NULL) {
    log_error(gc, zbw)("❌️  Failed to open resctrl (path: %s, flags: %s), are you root? Reason: ", path, flags);
    perror("fopen");
    crash();
  }

  return fd;
}

bool ZMemoryBandwidthMonitor::resctrl_check_status() {
  FILE* fd;
  char buf[512];
  bool result;
  
  resctrl_get_read_lock();
  fd = resctrl_open("/sys/fs/resctrl/info/last_cmd_status", "r");

  if(fgets(buf, 512, fd) == NULL) {
    log_error(gc, zbw)("❌️  could not read resctrl result");
    return false;
  }
  
  if(strcmp(buf, "ok\n") == 0) {
    result = true;
  } else {
    result = false;
    log_error(gc, zbw)("❌️  resctrl failed with reason: %s", buf);
  }

  fclose(fd);
  resctrl_release_lock();

  return result;
}

void ZMemoryBandwidthMonitor::resctrl_assign_cos(int group, pid_t pid) {
  FILE* fd;

  resctrl_get_write_lock();
  if(group == 0) {
    fd = resctrl_open("/sys/fs/resctrl/tasks", "rwa+");  
  } else {
    char path[1024];
    sprintf(path, "/sys/fs/resctrl/COS%d/tasks", group);
    fd = resctrl_open(path, "rwa+");
  }

  int wr = fprintf(fd, "%d\n", pid);
  if(wr < 0) {
    log_error(gc, zbw)("❌️  Failed to assign thread to cos in resctrl, reason: ");
    perror("fprintf");
    crash();
  }

  fclose(fd);
  resctrl_release_lock();

  if(!resctrl_check_status()) {
    crash();
  }

  log_debug(gc, zbw)("✔️  Assigned thread %d to bandwidth monitoring", pid);
}

long ZMemoryBandwidthMonitor::resctrl_read_mbm(int group) {
  FILE* fd;
  long res;

  resctrl_get_read_lock();
  if(group == 0) {
    fd = resctrl_open("/sys/fs/resctrl/tasks", "r");  
  } else {
    char path[1024];
    sprintf(path, "/sys/fs/resctrl/COS%d/mon_data/mon_L3_00/mbm_total_bytes", group);
    fd = resctrl_open(path, "r");
  }

  if(fscanf(fd, "%ld", &res) == 0) {
    log_error(gc, zbw)("❌️  Failed to read mbm_total_bytes");
  }

  fclose(fd);
  resctrl_release_lock();

  return res;
}

void ZMemoryBandwidthMonitor::add_thread(pid_t tid) {
  resctrl_assign_cos(2, tid);
}