/**
 * @file    mwrs_server.cpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */

#define MWRS_INCLUDE_SERVER
#include <mwrs.h>
#include "mwrs_messages.hpp"


#include <algorithm>
#include <atomic>
#include <iterator>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

#ifdef WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#endif


namespace
{

// Consts

const mwrs_size MWRS_SV_BUFFER_SIZE = 4096;



// Forward decl

struct mwrs_server_data;
struct mwrs_client_data;



// Platform-specific data

#ifdef WIN32

class WinEvent
{
public:
  WinEvent() : event(CreateEvent(NULL, TRUE, FALSE, NULL)) {}
  ~WinEvent() { CloseHandle(event); }

  operator HANDLE() { return event; }

  HANDLE event;
};


class WinClientThread
{
public:
  WinClientThread();
  ~WinClientThread();

  void interrupt();

  bool try_add_client(HANDLE pipe);


private:
  class ClientHandle
  {
  public:
    ClientHandle(WinClientThread * parent, HANDLE pipe);
    ~ClientHandle();

    void tick();
    void close();

    void read_completed();
    void write_completed();

    WinEvent read_event;
    WinEvent write_event;


  private:
    void on_read(mwrs_size readlen);


    WinClientThread * parent;

    mwrs_client_data * client = nullptr;

    HANDLE pipe = INVALID_HANDLE_VALUE;

    char read_buffer[MWRS_SV_BUFFER_SIZE];
    char write_buffer[MWRS_SV_BUFFER_SIZE];

    OVERLAPPED read_overlapped;
    OVERLAPPED write_overlapped;

    bool reading = false;
    bool writing = false;
  };
  // ClientHandle


  void run();


  mwrs_server_data * server;

  WinEvent wake_event;
  std::vector<std::unique_ptr<ClientHandle>> clients;

  std::thread thread;

  std::mutex mutex;
  std::vector<std::unique_ptr<ClientHandle>> pending_clients;
};
// WinClientThread

class WinAcceptThread
{
public:
  WinAcceptThread(mwrs_server_data * server);
  ~WinAcceptThread();

  void interrupt();


private:
  void run();

  mwrs_server_data * server;

  WinEvent wake_event;

  std::thread thread;
  std::vector<std::unique_ptr<WinClientThread>> client_threads;
};
// WinAcceptThread


struct mwrs_server_plat
{
  std::unique_ptr<WinAcceptThread> thread;
};

struct mwrs_client_plat
{
  HANDLE wake_event = INVALID_HANDLE_VALUE;
};

#endif



// Data structs

struct mwrs_server_data
{
  char name[MWRS_SERVER_NAME_MAX + 1];
  mwrs_sv_callbacks callbacks;

  mwrs_server_plat plat;
};

struct mwrs_client_data
{
  mwrs_sv_client client;
  mwrs_server_data * server;

  mwrs_client_plat plat;
};



// Platform-specific functions

mwrs_ret plat_server_start(mwrs_server_data * server);

void plat_server_stop(mwrs_server_data * server);

void plat_client_on_message_queued(mwrs_client_data * client);


// Functions

mwrs_client_data * server_on_client_connect(mwrs_server_data * server, int argc, char ** argv);

void server_on_client_disconnect(mwrs_server_data * server, mwrs_client_data * client);

void client_send_message(mwrs_client_data * client, mwrs_server_message * message);

void client_on_receive_message(mwrs_client_data * client, mwrs_client_message * message);



//



// Platform-specific implementation

#ifdef WIN32

WinClientThread::ClientHandle::ClientHandle(WinClientThread * parent, HANDLE pipe)
  : parent(parent), pipe(pipe)
{
}


WinClientThread::ClientHandle::~ClientHandle()
{
  // Should be called manually... just in case
  close();

  CancelIo(pipe);
  CloseHandle(pipe);
}


void WinClientThread::ClientHandle::close()
{
  if (client)
  {
    server_on_client_disconnect(parent->server, client);
    client = nullptr;
  }
}


void WinClientThread::ClientHandle::tick()
{
  for (;;)
  {
    if (!reading)
    {
      ZeroMemory(&read_overlapped, sizeof(read_overlapped));
      read_overlapped.hEvent = read_event;

      DWORD err = ERROR_SUCCESS;

      DWORD read_len;
      if (ReadFile(pipe, read_buffer, MWRS_SV_BUFFER_SIZE, &read_len, &read_overlapped) == 0)
        err = GetLastError();

      if (err == ERROR_IO_PENDING)
      {
        reading = true;
      }
      else if (err == ERROR_SUCCESS)
      {
        on_read(read_len);
        continue;
      }
      else
      {
        // TODO Error
      }
    }

    if (!writing)
    {
      // Check message queue
    }

    break;
  }
}
// Client tick


void WinClientThread::ClientHandle::read_completed()
{
  DWORD read_len, err = ERROR_SUCCESS;
  if (!GetOverlappedResult(pipe, &read_overlapped, &read_len, FALSE))
    err = GetLastError();

  if (err == ERROR_SUCCESS)
  {
    on_read(read_len);
  }
  else
  {
    // TODO Error
  }

  reading = false;
  ResetEvent(read_event);
}
// Client read_completed


void WinClientThread::ClientHandle::write_completed()
{
  DWORD write_len, err = ERROR_SUCCESS;
  if (!GetOverlappedResult(pipe, &write_overlapped, &write_len, FALSE))
    err = GetLastError();

  if (err != ERROR_SUCCESS)
  {
    // TODO Error
  }

  writing = false;
  ResetEvent(write_event);
}
// Client write_completed


void WinClientThread::ClientHandle::on_read(mwrs_size readlen)
{
  if (client)
  {
    // TODO receive message
  }
  else
  {
    // TODO read handshake

    client = server_on_client_connect(parent->server, 0, nullptr);
    if (client)
    {
      // Share wake event
      client->plat.wake_event = parent->wake_event;
    }
    else
    {
      // TODO error
    }
  }
}
// Client on_read


bool WinClientThread::try_add_client(HANDLE pipe)
{
  std::unique_lock<std::mutex>(mutex);

  if (pending_clients.size() + clients.size() >= 16) // TODO hardcoded
    return false;

  pending_clients.emplace_back(new ClientHandle(this, pipe));
  SetEvent(wake_event);
  return true;
}


void WinClientThread::run()
{
  HANDLE events[MAXIMUM_WAIT_OBJECTS];
  events[0] = wake_event;

  while (true) // TODO not interrupted
  {
    ResetEvent(wake_event);

    // Add new clients to list
    {
      std::unique_lock<std::mutex>(mutex);
      if (!pending_clients.empty())
      {
        std::move(std::begin(pending_clients), std::end(pending_clients), std::back_inserter(clients));
        pending_clients.clear();
      }
    }

    // TODO clear clients in error / disconnected (after tick?)

    for (int i = 0; i < clients.size(); ++i)
    {
      ClientHandle * cl = clients.at(i).get();
      cl->tick();
      events[1 + i * 2] = cl->read_event;
      events[2 + i * 2] = cl->write_event;
    }

    // wake_event for thread + read & write per client
    int num_events = (int)( 1 + clients.size() * 2 );
    DWORD dw = MsgWaitForMultipleObjectsEx(num_events, events, INFINITE, QS_ALLINPUT, 0);

    // error
    if (dw < WAIT_OBJECT_0 || dw > WAIT_OBJECT_0 + num_events)
    {
      // TODO error
    }

    // message
    else if (dw == WAIT_OBJECT_0 + num_events)
    {
      MSG msg;
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessage(&msg);
    }

    // wake_event
    else if (dw == WAIT_OBJECT_0)
    {
      // Event reset is handled by loop
      // Do nothing
    }

    // read or write event
    else
    {
      int client_num = (dw - WAIT_OBJECT_0 - 1) / 2;
      bool is_read = ((dw - WAIT_OBJECT_0 - 1) % 2) == 0;

      ClientHandle * client = clients.at(client_num).get();

      if (is_read)
      {
        client->read_completed();
      }
      else // write
      {
        client->write_completed();
      }
    }

  } // run loop

  // Close all clients on exit
  for (int i = 0; i < clients.size(); ++i)
    clients.at(i)->close();
}
// ClientThread run



void WinAcceptThread::interrupt()
{
  // TODO
  SetEvent(wake_event);
}
// AcceptThread interrupt


void WinAcceptThread::run()
{
  // Enough to hold "\\.\pipe\mwrs_" + server name + terminating null character
  TCHAR pipename[64 + MWRS_SERVER_NAME_MAX];
  _stprintf_s(pipename, 64 + MWRS_SERVER_NAME_MAX, TEXT("\\\\.\\pipe\\mwrs_%s"), server->name);

  // Manual reset event
  WinEvent accept_event;
  OVERLAPPED accept_overlapped;

  std::vector<std::unique_ptr<WinClientThread>> client_threads;

  for (;;)
  {
    HANDLE pipe = CreateNamedPipe(
      pipename,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES,
      MWRS_SV_BUFFER_SIZE,
      MWRS_SV_BUFFER_SIZE,
      0,
      NULL);

    if (pipe == INVALID_HANDLE_VALUE)
    {
      // TODO ???
      break;
    }

    ZeroMemory(&accept_overlapped, sizeof(accept_overlapped));

    ResetEvent(accept_event);
    accept_overlapped.hEvent = accept_event;

    if (ConnectNamedPipe(pipe, &accept_overlapped) != 0)
    {
      // TODO Critical error? should never happen
      CloseHandle(pipe);
      continue;
    }

    DWORD err = GetLastError();

    if (err == ERROR_IO_PENDING)
    {
      win_wait_event(accept_event);

      DWORD unused;
      if (GetOverlappedResult(pipe, &accept_overlapped, &unused, FALSE))
      {
        err = ERROR_PIPE_CONNECTED;
      }
      else
      {
        err = GetLastError();
      }
    }

    if (err == ERROR_PIPE_CONNECTED)
    {
      // Connected
      bool added = false;
      for (auto & client_thread : client_threads)
      {
        if (client_thread->try_add_client(pipe))
        {
          added = true;
          break;
        }
      }

      if (!added)
      {
        client_threads.emplace_back(new WinClientThread);
        if (!client_threads.back()->try_add_client(pipe))
        {
          // TODO error
        }
      }
    }
    else
    {
      // TODO The client could not connect, so close the pipe.
      CloseHandle(pipe);
    }
  }

  // Close client threads
  for (auto & client_thread : client_threads)
    client_thread->interrupt();

  client_threads.clear();
}
// AcceptThread run



mwrs_ret plat_server_start(mwrs_server_data * server)
{
  try
  {
    server->plat.thread.reset(new WinAcceptThread(server));
    return MWRS_SUCCESS;
  }
  catch (const std::exception &)
  {
    return MWRS_E_SYSTEM;
  }
}
// plat_server_start


void plat_server_stop(mwrs_server_data * server)
{
  // TODO
  server->plat.thread->interrupt();
  server->plat.thread.reset();
}
// plat_server_stop


void plat_client_on_message_queued(mwrs_client_data * client)
{
  SetEvent(client->plat.wake_event);
}
// plat_client_on_message_queued

#endif // WIN32

}
// unnamed ns
