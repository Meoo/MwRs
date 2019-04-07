
#define MWRS_INCLUDE_SERVER
#include <mwrs.h>

#include <atomic>
#include <cassert>
#include <csignal>
#include <thread>


namespace
{

std::atomic_bool stop_flag {false};
void sig_handler(int) { stop_flag = true; }


mwrs_ret my_connect(mwrs_sv_client * client, int argc, char ** argv)
{
  printf("Client connected\n");
  return MWRS_SUCCESS;
}

void my_disconnect(mwrs_sv_client * client)
{
  printf("Client disconnected\n");
}

mwrs_ret my_open(mwrs_sv_client * client, const char * id, mwrs_open_flags flags, mwrs_sv_res_open * open_out)
{
  printf("Client open\n");
  return MWRS_E_SERVERERR;
}

mwrs_ret my_stat(mwrs_sv_client * client, const char * id, mwrs_status * stat_out)
{
  printf("Client stat\n");
  return MWRS_E_SERVERERR;
}

} // namespace


int main(int argc, char ** argv)
{
  std::signal(SIGINT, sig_handler);

  mwrs_sv_callbacks sv_callbacks = {};

  sv_callbacks.connect = my_connect;
  sv_callbacks.disconnect = my_disconnect;
  sv_callbacks.open = my_open;
  sv_callbacks.stat = my_stat;

  printf("Server init\n");

  if (mwrs_sv_init("example-server", &sv_callbacks) != MWRS_SUCCESS)
    assert("Server init failed");

  printf("Server init OK\n");

  // TODO Poll and send events
  while (!stop_flag)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  printf("Server shutdown\n");
  mwrs_sv_shutdown();
  return 0;
} // main