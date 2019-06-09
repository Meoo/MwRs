
#define MWRS_INCLUDE_SERVER
#include <mwrs.h>

#include <atomic>
#include <cassert>
#include <csignal>
#include <thread>

#include <fcntl.h>
#include <io.h>


namespace
{

std::atomic_bool stop_flag{false};
void sig_handler(int) { stop_flag = true; }


mwrs_ret my_connect(mwrs_sv_client * client, int argc, const char ** argv)
{
  printf("Client connected\n");

  for (int i = 0; i < argc; ++i)
  {
    printf("Arg %d : %s\n", i, argv[i]);
  }

  return MWRS_SUCCESS;
}

void my_disconnect(mwrs_sv_client * client) { printf("Client disconnected\n"); }

mwrs_ret my_open(mwrs_sv_client * client, const char * id, mwrs_open_flags flags,
                 mwrs_sv_res_open * open_out)
{
  int fd = _open(id, _O_RDONLY | _O_BINARY);

  if (fd == -1)
  {
    printf("Open error %s\n", strerror(errno));
    return MWRS_E_NOTFOUND;
  }

  open_out->type = MWRS_SV_FD;
  open_out->fd   = fd;

  return MWRS_SUCCESS;
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

  sv_callbacks.connect    = my_connect;
  sv_callbacks.disconnect = my_disconnect;
  sv_callbacks.open       = my_open;
  sv_callbacks.stat       = my_stat;

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
