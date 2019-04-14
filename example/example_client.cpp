
#include <mwrs.h>

#include <cassert>
#include <cstdio>

#include <thread>


int main(int argc, char** argv)
{
  printf("Client init...\n");

  if (mwrs_init("example-server", 0, nullptr) != MWRS_SUCCESS)
  {
    printf("Client init failed\n");
    return 1;
  }

  printf("Client init OK\n");

  mwrs_res res {};
  mwrs_ret ret = mwrs_open("C:/Test.txt", MWRS_OPEN_READ, &res);

  if (ret == MWRS_SUCCESS)
  {
    printf("Open OK\n");

    char buf[128] {};
    mwrs_size sz = 128;
    if (mwrs_read(&res, buf, &sz) == MWRS_SUCCESS)
      printf("Data: %s\n", buf);
  }
  else
    printf("Open error %d\n", ret);

  mwrs_close(&res);

  printf("Begin open loop\n");
  for (int i = 0; i < 100; ++i)
  {
    mwrs_res r{};
    ret = mwrs_open("C:/Test.txt", MWRS_OPEN_READ, &r);
    if (ret != MWRS_SUCCESS)
      printf("Open error %d\n", ret);
    mwrs_close(&r);
  }
  printf("End open loop\n");

  std::this_thread::sleep_for(std::chrono::seconds(3));

  printf("Client shutdown\n");
  mwrs_shutdown();
  return 0;
} // main
