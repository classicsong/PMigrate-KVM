#include "time.h"

struct debug_struct{
  long mem_dirty;
  long disk_dirty;
  long mem_iternum;
  long disk_iternum;
  long mem_datasent;
  long disk_datasent;
  long zeropage;
  struct timeval down_tv;
  struct timeval start_tv;
  struct timeval end_tv;
};

struct debug_struct s_debug;
