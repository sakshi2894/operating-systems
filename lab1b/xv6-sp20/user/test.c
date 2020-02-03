#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "checking for pid %d ",  getpid());
  printf(1, "return value: %d", getfilenum(getpid()));
  exit();
}
