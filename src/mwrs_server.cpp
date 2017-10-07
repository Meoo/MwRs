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
#include <list>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <set>

#ifdef WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#endif


namespace
{


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
  class ClientHandle
  {
  public:
    ClientHandle(WinClientThread * parent, HANDLE pipe);
    ~ClientHandle();

    void queue_message(const mwrs_sv_message * message); // TODO enqueue and raise wake_event

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
    std::mutex mutex;

    std::set<HANDLE> sent_handles;

    mwrs_cl_message read_message;
    std::list<mwrs_sv_message> write_queue;

    OVERLAPPED read_overlapped;
    OVERLAPPED write_overlapped;

    bool reading = false;
    bool writing = false;
  };
  // ClientHandle


  WinClientThread();
  ~WinClientThread();

  void interrupt();

  bool try_add_client(HANDLE pipe);


private:
  void run();


  mwrs_server_data * server;

  WinEvent wake_event;
  std::vector<std::unique_ptr<ClientHandle>> clients;

  std::thread thread;
  std::atomic_bool stop_flag {false};

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
  std::atomic_bool stop_flag {false};

  std::vector<std::unique_ptr<WinClientThread>> client_threads;
};
// WinAcceptThread


struct mwrs_server_plat
{
  std::unique_ptr<WinAcceptThread> thread;
};

struct mwrs_client_plat
{
  WinClientThread::ClientHandle * handle = nullptr;
};

#endif



// Data structs

struct mwrs_server_data
{
  char name[MWRS_SERVER_NAME_MAX + 1];
  mwrs_sv_callbacks callbacks;

  std::mutex mutex;
  std::set<std::unique_ptr<mwrs_client_data> > clients;
  int next_client_id;

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

void plat_client_send_message(mwrs_client_data * client, const mwrs_sv_message * message);


// Functions

mwrs_client_data * server_on_client_connect(mwrs_server_data * server, int argc, char ** argv)
{
  if (!server)
    return nullptr; // TODO error code somehow?

  std::unique_ptr<mwrs_client_data> client(new mwrs_client_data);
  client->server = server;
  client->client.userdata = nullptr;

  {
    std::unique_lock<std::mutex> lock(server->mutex);
    client->client.id = server->next_client_id++;

    if (server->callbacks.connect)
    {
      // TODO args
      if (server->callbacks.connect(&client->client, 0, nullptr) != MWRS_SUCCESS)
      {
        return nullptr; // TODO error code somehow?
      }
    }

    server->clients.emplace(std::move(client));
  }
}

void server_on_client_disconnect(mwrs_server_data * server, mwrs_client_data * client)
{
  if (!server || !client)
    return;

  std::unique_lock<std::mutex> lock(server->mutex);

  auto it = std::find_if(server->clients.begin(), server->clients.end(),
    [client](const auto & v)
    {
      return v.get() == client;
    });

  if (it != server->clients.end())
  {
    // TODO unlock & error
  }
  else
  {
    if (server->callbacks.disconnect)
      server->callbacks.disconnect(&client->client);

    server->clients.erase(it);
  }
}

mwrs_ret server_on_event(mwrs_server_data * server, const char * id, mwrs_event_type type);

void client_on_receive_message(mwrs_client_data * client, const mwrs_cl_message * message);



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

  for (auto handle : sent_handles)
    CloseHandle(handle);
  sent_handles.clear();

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


void WinClientThread::ClientHandle::queue_message(const mwrs_sv_message * message)
{
  {
    std::unique_lock<std::mutex>(mutex);
    write_queue.push_back(*message);
  }
  SetEvent(parent->wake_event);
}


void WinClientThread::ClientHandle::tick()
{
  for (;;)
  {
    if (!reading)
    {
      ZeroMemory(&read_message, sizeof(read_message));
      ZeroMemory(&read_overlapped, sizeof(read_overlapped));
      read_overlapped.hEvent = read_event;

      DWORD err = ERROR_SUCCESS;

      DWORD read_len;
      if (ReadFile(pipe, &read_message, sizeof(read_message), &read_len, &read_overlapped) == 0)
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

    if (!writing && !write_queue.empty())
    {
      ZeroMemory(&write_overlapped, sizeof(write_overlapped));
      write_overlapped.hEvent = write_event;

      mwrs_sv_message & send_message = write_queue.front();

      DWORD err = ERROR_SUCCESS;

      DWORD write_len;
      if (WriteFile(pipe, &send_message, sizeof(send_message), &write_len, &write_overlapped) == 0)
        err = GetLastError();

      if (err == ERROR_IO_PENDING)
      {
        writing = true;
      }
      else if (err == ERROR_SUCCESS)
      {
        // Pop sent message
        std::unique_lock<std::mutex>(mutex);
        write_queue.pop_front();
        continue;
      }
      else
      {
        // TODO Error
      }
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

  {
    // Pop sent message
    std::unique_lock<std::mutex>(mutex);
    write_queue.pop_front();
  }

  writing = false;
  ResetEvent(write_event);
}
// Client write_completed


void WinClientThread::ClientHandle::on_read(mwrs_size readlen)
{
  if (readlen != sizeof(mwrs_cl_message))
  {
    // TODO receive error
  }

  switch (read_message.type)
  {
  case MWRS_MSG_CL_WIN_CONSUME_HANDLE:
    {
      auto it = sent_handles.find((HANDLE)read_message.win_consume_handle.handle);
      if (it != sent_handles.end())
      {
        sent_handles.erase(it);
      }
      else
      {
        // TODO error
      }
    }
    break; // MWRS_MSG_CL_WIN_CONSUME_HANDLE

  case MWRS_MSG_CL_WIN_HANDSHAKE:
    if (!client)
    {
      if (read_message.type != MWRS_MSG_CL_WIN_HANDSHAKE)
      {
        // TODO error
      }

      // TODO read handshake

      client = server_on_client_connect(parent->server, 0, nullptr);
      if (client)
      {
        client->plat.handle = this;
      }
      else
      {
        // TODO error
      }
    }
    else
    {
      // TODO error
    }
    break; // MWRS_MSG_CL_WIN_HANDSHAKE

  default:
    if (client)
    {
      client_on_receive_message(client, &read_message);
    }
    else
    {
      // TODO error
    }
  } // switch message type
}
// Client on_read



WinClientThread::WinClientThread()
  : thread([this]() { run(); })
{
}

WinClientThread::~WinClientThread()
{
  interrupt();
}

void WinClientThread::interrupt()
{
  if (thread.joinable())
  {
    stop_flag = true;
    SetEvent(wake_event);
    thread.join();
  }
}


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

  while (!stop_flag)
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



WinAcceptThread::WinAcceptThread(mwrs_server_data * server)
  : server(server), thread([this]() { run(); })
{
}

WinAcceptThread::~WinAcceptThread()
{
  interrupt();
}

void WinAcceptThread::interrupt()
{
  if (thread.joinable())
  {
    stop_flag = true;
    SetEvent(wake_event);
    thread.join();
  }
}


void WinAcceptThread::run()
{
  // Enough to hold "\\.\pipe\mwrs_" + server name + terminating null character
  TCHAR pipename[64 + MWRS_SERVER_NAME_MAX];
  _stprintf_s(pipename, 64 + MWRS_SERVER_NAME_MAX, TEXT("\\\\.\\pipe\\mwrs_%s"), server->name);

  // Manual reset event
  WinEvent accept_event;
  OVERLAPPED accept_overlapped;

  std::vector<std::unique_ptr<WinClientThread>> client_threads;

  while (!stop_flag)
  {
    HANDLE pipe = CreateNamedPipe(
      pipename,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES,
      sizeof(mwrs_sv_message),
      sizeof(mwrs_cl_message),
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
        try
        {
          client_threads.emplace_back(new WinClientThread);
          if (!client_threads.back()->try_add_client(pipe))
          {
            // TODO error
          }
        }
        catch (const std::exception &)
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


void plat_client_send_message(mwrs_client_data * client, const mwrs_sv_message * message)
{
  client->plat.handle->queue_message(message);
}
// plat_client_send_message

#endif // WIN32



// Server instance holder
std::unique_ptr<mwrs_server_data> instance;

}
// unnamed ns


// API implementation

mwrs_ret mwrs_sv_init(const char * server_name, mwrs_sv_callbacks * callbacks)
{
  if (::instance)
    return MWRS_E_ALREADY;

  if (!server_name || !callbacks)
    return MWRS_E_ARGS;

  ::instance.reset(new mwrs_server_data);
  std::memset(::instance.get(), 0, sizeof(mwrs_server_data));

  std::strncpy(::instance->name, server_name, MWRS_SERVER_NAME_MAX);
  ::instance->callbacks = *callbacks;

  ::instance->next_client_id = 1;

  return plat_server_start(::instance.get());
}

mwrs_ret mwrs_sv_shutdown()
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  plat_server_stop(::instance.get());
  ::instance.reset();

  return MWRS_SUCCESS;
}

mwrs_ret mwrs_sv_push_event(const char * id, mwrs_event_type type)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  if (!id)
    return MWRS_E_ARGS;

  return server_on_event(::instance.get(), id, type);
}
