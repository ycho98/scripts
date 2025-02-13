#define _GNU_SOURCE

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static unsigned long long procid;

static void sleep_ms(uint64_t ms)
{
	  usleep(ms * 1000);
}

static uint64_t current_time_ms(void)
{
	  struct timespec ts;
	    if (clock_gettime(CLOCK_MONOTONIC, &ts))
		        exit(1);
	      return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

#define BITMASK(bf_off, bf_len) (((1ull << (bf_len)) - 1) << (bf_off))
#define STORE_BY_BITMASK(type, htobe, addr, val, bf_off, bf_len)               \
	  *(type*)(addr) =                                                             \
	        htobe((htobe(*(type*)(addr)) & ~BITMASK((bf_off), (bf_len))) |           \
				            (((type)(val) << (bf_off)) & BITMASK((bf_off), (bf_len))))

static bool write_file(const char* file, const char* what, ...)
{
	  char buf[1024];
	    va_list args;
	      va_start(args, what);
	        vsnprintf(buf, sizeof(buf), what, args);
		  va_end(args);
		    buf[sizeof(buf) - 1] = 0;
		      int len = strlen(buf);
		        int fd = open(file, O_WRONLY | O_CLOEXEC);
			  if (fd == -1)
				      return false;
			    if (write(fd, buf, len) != len) {
				        int err = errno;
					    close(fd);
					        errno = err;
						    return false;
						      }
			      close(fd);
			        return true;
}

static int inject_fault(int nth)
{
	  int fd;
	    fd = open("/proc/thread-self/fail-nth", O_RDWR);
	      if (fd == -1)
		          exit(1);
	        char buf[16];
		  sprintf(buf, "%d", nth + 1);
		    if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf))
			        exit(1);
		      return fd;
}

static void kill_and_wait(int pid, int* status)
{
	  kill(-pid, SIGKILL);
	    kill(pid, SIGKILL);
	      for (int i = 0; i < 100; i++) {
		          if (waitpid(-1, status, WNOHANG | __WALL) == pid)
				        return;
			      usleep(1000);
			        }
	        DIR* dir = opendir("/sys/fs/fuse/connections");
		  if (dir) {
			      for (;;) {
				            struct dirent* ent = readdir(dir);
					          if (!ent)
							          break;
						        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
								        continue;
							      char abort[300];
							            snprintf(abort, sizeof(abort), "/sys/fs/fuse/connections/%s/abort",
										                   ent->d_name);
								          int fd = open(abort, O_WRONLY);
									        if (fd == -1) {
											        continue;
												      }
										      if (write(fd, abort, 1) < 0) {
											            }
										            close(fd);
											        }
			          closedir(dir);
				    } else {
					      }
		    while (waitpid(-1, status, __WALL) != pid) {
			      }
}

static void setup_test()
{
	  prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
	    setpgrp();
	      write_file("/proc/self/oom_score_adj", "1000");
}

static void setup_fault()
{
	  static struct {
		      const char* file;
		          const char* val;
			      bool fatal;
			        } files[] = {
					      {"/sys/kernel/debug/failslab/ignore-gfp-wait", "N", true},
					            {"/sys/kernel/debug/fail_futex/ignore-private", "N", false},
						          {"/sys/kernel/debug/fail_page_alloc/ignore-gfp-highmem", "N", false},
							        {"/sys/kernel/debug/fail_page_alloc/ignore-gfp-wait", "N", false},
								      {"/sys/kernel/debug/fail_page_alloc/min-order", "0", false},
								        };
	    unsigned i;
	      for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
		          if (!write_file(files[i].file, files[i].val)) {
				        if (files[i].fatal)
						        exit(1);
					    }
			    }
}

static void execute_one(void);

#define WAIT_FLAGS __WALL

static void loop(void)
{
	  int iter = 0;
	    for (;; iter++) {
		        int pid = fork();
			    if (pid < 0)
				          exit(1);
			        if (pid == 0) {
					      setup_test();
					            execute_one();
						          exit(0);
							      }
				    int status = 0;
				        uint64_t start = current_time_ms();
					    for (;;) {
						          if (waitpid(-1, &status, WNOHANG | WAIT_FLAGS) == pid)
								          break;
							        sleep_ms(1);
								      if (current_time_ms() - start < 5 * 1000)
									              continue;
								            kill_and_wait(pid, &status);
									          break;
										      }
					      }
}

#ifndef __NR_bpf
#define __NR_bpf 321
#endif

uint64_t r[2] = {0xffffffffffffffff, 0xffffffffffffffff};

void execute_one(void)
{
	  intptr_t res = 0;
	    *(uint32_t*)0x20000200 = 0x18;
	      *(uint32_t*)0x20000204 = 3;
	        *(uint64_t*)0x20000208 = 0x200000c0;
		  *(uint8_t*)0x200000c0 = 0x18;
		    STORE_BY_BITMASK(uint8_t, , 0x200000c1, 0, 0, 4);
		      STORE_BY_BITMASK(uint8_t, , 0x200000c1, 0, 4, 4);
		        *(uint16_t*)0x200000c2 = 0;
			  *(uint32_t*)0x200000c4 = 0;
			    *(uint8_t*)0x200000c8 = 0;
			      *(uint8_t*)0x200000c9 = 0;
			        *(uint16_t*)0x200000ca = 0;
				  *(uint32_t*)0x200000cc = 0;
				    *(uint8_t*)0x200000d0 = 0x95;
				      *(uint8_t*)0x200000d1 = 0;
				        *(uint16_t*)0x200000d2 = 0;
					  *(uint32_t*)0x200000d4 = 0;
					    *(uint64_t*)0x20000210 = 0x20000080;
					      memcpy((void*)0x20000080, "GPL\000", 4);
					        *(uint32_t*)0x20000218 = 0;
						  *(uint32_t*)0x2000021c = 0;
						    *(uint64_t*)0x20000220 = 0;
						      *(uint32_t*)0x20000228 = 0;
						        *(uint32_t*)0x2000022c = 0;
							  *(uint8_t*)0x20000230 = 0;
							    *(uint8_t*)0x20000231 = 0;
							      *(uint8_t*)0x20000232 = 0;
							        *(uint8_t*)0x20000233 = 0;
								  *(uint8_t*)0x20000234 = 0;
								    *(uint8_t*)0x20000235 = 0;
								      *(uint8_t*)0x20000236 = 0;
								        *(uint8_t*)0x20000237 = 0;
									  *(uint8_t*)0x20000238 = 0;
									    *(uint8_t*)0x20000239 = 0;
									      *(uint8_t*)0x2000023a = 0;
									        *(uint8_t*)0x2000023b = 0;
										  *(uint8_t*)0x2000023c = 0;
										    *(uint8_t*)0x2000023d = 0;
										      *(uint8_t*)0x2000023e = 0;
										        *(uint8_t*)0x2000023f = 0;
											  *(uint32_t*)0x20000240 = 0;
											    *(uint32_t*)0x20000244 = 2;
											      *(uint32_t*)0x20000248 = -1;
											        *(uint32_t*)0x2000024c = 8;
												  *(uint64_t*)0x20000250 = 0;
												    *(uint32_t*)0x20000258 = 0;
												      *(uint32_t*)0x2000025c = 0x10;
												        *(uint64_t*)0x20000260 = 0;
													  *(uint32_t*)0x20000268 = 0;
													    *(uint32_t*)0x2000026c = 0;
													      *(uint32_t*)0x20000270 = 0;
													        res = syscall(__NR_bpf, 5ul, 0x20000200ul, 0x78ul);
														  if (res != -1)
															      r[0] = res;
														    *(uint64_t*)0x200000c0 = 0x20000080;
														      memcpy((void*)0x20000080, "sched_switch\000", 13);
														        *(uint32_t*)0x200000c8 = r[0];
															  res = syscall(__NR_bpf, 0x11ul, 0x200000c0ul, 0x10ul);
															    if (res != -1)
																        r[1] = res;
															      syscall(__NR_close, r[0]);
															        inject_fault(0);
																  syscall(__NR_close, r[1]);
}
int main(void)
{
	  syscall(__NR_mmap, 0x1ffff000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
	    syscall(__NR_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x32ul, -1, 0ul);
	      syscall(__NR_mmap, 0x21000000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
	        setup_fault();
		  for (procid = 0; procid < 6; procid++) {
			      if (fork() == 0) {
				            loop();
					        }
			        }
		    sleep(1000000);
		      return 0;
}

