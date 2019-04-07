
#include <mwrs.h>

#include <cassert>
#include <cstdio>


int main(int argc, char ** argv)
{
  printf("Client init\n");

  if (mwrs_init("example-server", 0, nullptr) != MWRS_SUCCESS)
    assert("Client init failed");

  printf("Client init OK\n");

  printf("Client shutdown\n");
  mwrs_shutdown();
  return 0;
} // main
