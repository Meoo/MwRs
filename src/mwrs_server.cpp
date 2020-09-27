/**
 * @file    mwrs_server.cpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */

#define MWRS_INCLUDE_SERVER
#include "mwrs_messages.hpp"
#include <mwrs_server.h>


#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#  define VC_EXTRALEAN
#  define WIN32_LEAN_AND_MEAN
#  include <io.h>
#  include <tchar.h>
#  include <windows.h>
#endif


namespace
{

const DWORD pipeBufferSize = 4096;

static_assert(pipeBufferSize >= sizeof(mwrs_cl_message), "");
static_assert(pipeBufferSize >= sizeof(mwrs_sv_message), "");


// Forward decl

struct mwrs_server_data;
struct mwrs_client_data;


// Platform-specific data

#ifdef _WIN32

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

    void queue_message(mwrs_sv_message * message);

    void tick();
    void close();

    void read_completed();
    void write_completed();

    WinEvent read_event;
    WinEvent write_event;

    bool disconnected = false;

    HANDLE process = INVALID_HANDLE_VALUE;


   private:
    void on_read(mwrs_size readlen);


    WinClientThread * parent;

    mwrs_client_data * client = nullptr;

    HANDLE pipe = INVALID_HANDLE_VALUE;
    std::mutex mutex;

    mwrs_cl_message read_message_head{};
    mwrs_cl_message * read_message = nullptr;
    std::size_t read_offset;

    std::list<mwrs_sv_message *> write_queue;

    OVERLAPPED read_overlapped{};
    OVERLAPPED write_overlapped{};

    bool reading = false;
    bool writing = false;
  };
  // ClientHandle


  WinClientThread(mwrs_server_data * server);
  ~WinClientThread();

  void interrupt();

  bool try_add_client(HANDLE pipe);


 private:
  void run();


  mwrs_server_data * server = nullptr;

  WinEvent wake_event;
  std::vector<std::unique_ptr<ClientHandle>> clients;

  std::thread thread;
  std::atomic_bool stop_flag{false};

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

  mwrs_server_data * server = nullptr;

  WinEvent wake_event;

  std::thread thread;
  std::atomic_bool stop_flag{false};

  // std::vector<std::unique_ptr<WinClientThread>> client_threads;
};
// WinAcceptThread


mwrs_ret fill_win_handle_from_res_open(const mwrs_client_data * client,
                                       const mwrs_sv_res_open * res_open,
                                       mwrs_sv_msg_common_response * response_out);


struct mwrs_server_plat
{
  std::unique_ptr<WinAcceptThread> thread;
};

struct mwrs_client_plat
{
  WinClientThread::ClientHandle * handle = nullptr;
};

#endif // _WIN32


// Data structs

struct mwrs_server_data
{
  char name[MWRS_SERVER_NAME_MAX]{0};
  mwrs_sv_callbacks callbacks;

  std::mutex mutex;
  std::set<std::unique_ptr<mwrs_client_data>> clients;
  int next_client_id = 1;

  mwrs_watcher_id next_watcher_id = 1;

  struct watcher_instance
  {
    mwrs_client_data * client;
    mwrs_watcher_id id;
  };
  std::unordered_map<std::string, std::set<watcher_instance>> watcher_data;

  mwrs_server_plat plat;
};

struct mwrs_client_data
{
  mwrs_sv_client client{};
  mwrs_server_data * server = nullptr;

  mwrs_client_plat plat;
};


// Platform-specific functions

mwrs_ret plat_server_start(mwrs_server_data * server);

void plat_server_stop(mwrs_server_data * server);

void plat_client_queue_message(mwrs_client_data * client, mwrs_sv_message * message);


// Functions

void * message_alloc(size_t size) { return new char[size]{}; }

void message_free(void* message) { delete[] message; }


mwrs_ret server_on_client_connect(mwrs_server_data * server, int argc, const char ** argv,
                                  mwrs_client_data ** client_out)
{
  if (!server || !client_out)
    return MWRS_E_ARGS;

  std::unique_ptr<mwrs_client_data> client(new mwrs_client_data);
  client->server          = server;
  client->client.userdata = nullptr;

  {
    std::unique_lock<std::mutex> lock(server->mutex);
    client->client.id = server->next_client_id++;

    if (server->callbacks.connect)
    {
      mwrs_ret ret;
      if ((ret = server->callbacks.connect(&client->client, argc, argv)) != MWRS_SUCCESS)
        return ret;
    }

    auto client_ptr = client.get();
    server->clients.emplace(std::move(client));
    *client_out = client_ptr;
  }
  return MWRS_SUCCESS;
}
// server_on_client_connect

void server_on_client_disconnect(mwrs_server_data * server, mwrs_client_data * client)
{
  if (!server || !client)
    return;

  std::unique_lock<std::mutex> lock(server->mutex);

  auto it = std::find_if(
      server->clients.begin(), server->clients.end(),
      [client](const std::unique_ptr<mwrs_client_data> & v) { return v.get() == client; });

  if (it == server->clients.end())
  {
    // TODO unlock & error
    assert(0 && "Client not found in server");
  }
  else
  {
    // TODO Clear all watchers

    if (server->callbacks.disconnect)
      server->callbacks.disconnect(&client->client);

    server->clients.erase(it);
  }
}
// server_on_client_disconnect

mwrs_ret server_on_event(mwrs_server_data * server, const char * id, mwrs_event_type type)
{
  return MWRS_E_SERVERERR; // TODO
}
// server_on_event

void client_on_receive_message(mwrs_client_data * client, const mwrs_cl_message * message)
{
  mwrs_sv_message * response = nullptr;
  switch (message->type)
  {
  case MWRS_MSG_CL_OPEN:
  case MWRS_MSG_CL_WATCH:
  case MWRS_MSG_CL_OPEN_WATCH:
  case MWRS_MSG_CL_STAT:
  case MWRS_MSG_CL_STAT_WATCH:
  {
    mwrs_cl_msg_resource_request * resource_request = (mwrs_cl_msg_resource_request *)message;
    mwrs_sv_msg_common_response * common_response =
        (mwrs_sv_msg_common_response *)message_alloc(sizeof(mwrs_sv_msg_common_response));
    common_response->type = MWRS_MSG_SV_COMMON_RESPONSE;
    common_response->length = sizeof(mwrs_sv_msg_common_response);
    response                = (mwrs_sv_message *)common_response;
    // Watch first
    switch (message->type)
    {
    case MWRS_MSG_CL_WATCH:
    case MWRS_MSG_CL_OPEN_WATCH:
    case MWRS_MSG_CL_STAT_WATCH:
      // TODO watch
      break;
    default: break;
    }
    // Open
    switch (message->type)
    {
    case MWRS_MSG_CL_OPEN:
    case MWRS_MSG_CL_OPEN_WATCH:
    {
      mwrs_sv_res_open res_open{};
      common_response->status =
          client->server->callbacks.open(&client->client, &resource_request->resource_id, resource_request->flags, &res_open);

      if (common_response->status == MWRS_SUCCESS)
      {
        common_response->open_flags = resource_request->flags;
        // TODO platform dependant, fixme
        // Fill file descriptor after open_flags (can be used by res open) TODO use argument?
        common_response->status = fill_win_handle_from_res_open(client, &res_open, common_response);
      }
    }
    default: break;
    }
    // Stat
    switch (message->type)
    {
    case MWRS_MSG_CL_STAT:
    case MWRS_MSG_CL_STAT_WATCH:
    {
      mwrs_status res_stat{};
      common_response->status = client->server->callbacks.stat(
          &client->client, &resource_request->resource_id, &res_stat);
      break;
    }
    default: break;
    }
    break;
  }
  case MWRS_MSG_CL_WATCHER_OPEN:
  case MWRS_MSG_CL_CLOSE_WATCHER: break;

  default:
    // TODO error
    assert(0 && "Invalid message type");
  }

  if (response)
    plat_client_queue_message(client, response);
  else
    assert(0 && "No response to client message");
}
// client_on_receive_message


//


// Platform-specific implementation

#ifdef _WIN32

mwrs_win_handle_data to_mwrs_handle(HANDLE win_handle)
{
  static_assert(sizeof(mwrs_win_handle_data) == 4, "mwrs_win_handle_data must be 4 bytes");
  return reinterpret_cast<mwrs_win_handle_data>(win_handle);
}

WinClientThread::ClientHandle::ClientHandle(WinClientThread * parent, HANDLE pipe)
    : parent(parent), pipe(pipe)
{
}


WinClientThread::ClientHandle::~ClientHandle()
{
  // Should be called manually... just in case
  close();

  CloseHandle(process);

  CancelIo(pipe);
  CloseHandle(pipe);

  // Clear messages
  while (!write_queue.empty())
  {
    message_free(write_queue.front());
    write_queue.pop_front();
  }

  if (read_message)
  {
    message_free(read_message);
    read_message = nullptr;
  }
}


void WinClientThread::ClientHandle::close()
{
  if (client)
  {
    server_on_client_disconnect(parent->server, client);
    client = nullptr;
  }
}


void WinClientThread::ClientHandle::queue_message(mwrs_sv_message * message)
{
  {
    std::unique_lock<std::mutex> lock(mutex);
    write_queue.push_back(message);
  }
  SetEvent(parent->wake_event);
}


void WinClientThread::ClientHandle::tick()
{
  if (disconnected)
    return;

  for (;;)
  {
    if (!reading)
    {
      ZeroMemory(&read_overlapped, sizeof(read_overlapped));
      read_overlapped.hEvent = read_event;

      DWORD err = ERROR_SUCCESS;

      DWORD read_len;

      if (!read_message)
      {
        // Read message head
        ZeroMemory(&read_message_head, sizeof(read_message_head));
        if (ReadFile(pipe, &read_message_head, sizeof(read_message_head), &read_len,
                     &read_overlapped) == 0)
          err = GetLastError();
      }
      else
      {
        // Read message body
        if (ReadFile(pipe, (void*)((char*)read_message + read_offset), read_message_head.length - read_offset,
                     &read_len, &read_overlapped) == 0)
          err = GetLastError();
      }

      if (err == ERROR_IO_PENDING)
      {
        reading = true;
      }
      else if (err == ERROR_SUCCESS)
      {
        on_read(read_len);
        ResetEvent(read_event);
        continue;
      }
      else if (err == ERROR_BROKEN_PIPE)
      {
        disconnected = true;
        break;
      }
      else
      {
        // TODO Error
        assert(0 && "Read error");
      }
    }

    if (!writing && !write_queue.empty())
    {
      ZeroMemory(&write_overlapped, sizeof(write_overlapped));
      write_overlapped.hEvent = write_event;

      mwrs_sv_message * send_message = write_queue.front();

      DWORD err = ERROR_SUCCESS;

      DWORD write_len;
      if (WriteFile(pipe, send_message, send_message->length, &write_len, &write_overlapped) == 0)
        err = GetLastError();

      if (err == ERROR_IO_PENDING)
      {
        writing = true;
      }
      else if (err == ERROR_SUCCESS)
      {
        {
          // Pop sent message
          std::unique_lock<std::mutex> lock(mutex);
          write_queue.pop_front();
        }
        message_free(send_message);
        ResetEvent(write_event);
        continue;
      }
      else if (err == ERROR_BROKEN_PIPE)
      {
        disconnected = true;
        break;
      }
      else
      {
        // TODO Error
        assert(0 && "Write error");
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
  else if (err == ERROR_BROKEN_PIPE)
  {
    disconnected = true;
    return;
  }
  else
  {
    // TODO Error
    assert(0 && "Async read error");
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

  if (err == ERROR_BROKEN_PIPE)
  {
    disconnected = true;
    return;
  }
  else if (err != ERROR_SUCCESS)
  {
    // TODO Error
    assert(0 && "Async write error");
  }

  mwrs_sv_message * message = write_queue.front();
  {
    // Pop sent message
    std::unique_lock<std::mutex> lock(mutex);
    write_queue.pop_front();
  }
  message_free(message);

  writing = false;
  ResetEvent(write_event);
}
// Client write_completed


void WinClientThread::ClientHandle::on_read(mwrs_size readlen)
{
  if (!read_message)
  {
    // Message head
    if (readlen != sizeof(mwrs_cl_message))
    {
      // TODO receive error
      assert(0 && "Receive error");
    }

    read_message = (mwrs_cl_message *)message_alloc(read_message_head.length);
    std::memcpy(read_message, &read_message_head, sizeof(read_message));
    read_offset = sizeof(read_message);
    return;
  }

  // Message body
  read_offset += readlen;
  if (read_offset < read_message_head.length)
  {
    // Read is not complete
    return;
  }

  // Message is complete
  assert(read_offset == read_message_head.length);

  switch (read_message->type)
  {
  case MWRS_MSG_CL_WIN_HANDSHAKE:
    if (!client)
    {
      mwrs_cl_win_handshake * win_handshake = (mwrs_cl_win_handshake *)read_message;
      mwrs_sv_win_handshake_ack * handshake_ack =
          (mwrs_sv_win_handshake_ack *)message_alloc(sizeof(mwrs_sv_win_handshake_ack));
      handshake_ack->type = MWRS_MSG_SV_WIN_HANDSHAKE_ACK;
      handshake_ack->length = sizeof(mwrs_sv_win_handshake_ack);

      if (win_handshake->mwrs_version != MWRS_VERSION)
      {
        handshake_ack->status = MWRS_E_NOTSUPPORTED;
      }
      else
      {
        DWORD process_id = win_handshake->process_id;
        process          = OpenProcess(PROCESS_DUP_HANDLE, FALSE, process_id);

        // Documentation says NULL
        if (process == INVALID_HANDLE_VALUE || process == NULL)
        {
          // TODO error
          assert(0 && "Failed to open client process");
        }

        std::array<const char *, 128> argv_ptr{}; // TODO hardcoded
        int argc    = win_handshake->argc;
        char * argv = &win_handshake->argv;

        if (argc > argv_ptr.size())
        {
          // TODO warning
          argc = argv_ptr.size();
        }

        for (int i = 0; i < argc; ++i)
        {
          argv_ptr[i] = argv;
          argv += std::strlen(argv) + 1;
        }

        mwrs_ret ret = server_on_client_connect(parent->server, argc, argv_ptr.data(), &client);

        if (ret == MWRS_SUCCESS)
        {
          client->plat.handle = this;
        }
        else
        {
          // TODO error
          assert(0 && "Client creation error");
        }

        handshake_ack->status = ret;
      }

      queue_message((mwrs_sv_message *)handshake_ack);
    }
    else
    {
      // TODO error
      assert(0 && "Received handshake twice");
    }
    break; // MWRS_MSG_CL_WIN_HANDSHAKE

  default:
    if (client)
    {
      client_on_receive_message(client, read_message);
    }
    else
    {
      // TODO error
      assert(0 && "Must perform handshake first");
    }
  } // switch message type

  message_free(read_message);
  read_message = nullptr;
}
// Client on_read


WinClientThread::WinClientThread(mwrs_server_data * server) : server(server)
{
  // Do not call before all members are initialized
  std::thread t([this]() { run(); });
  thread.swap(t);
}

WinClientThread::~WinClientThread() { interrupt(); }

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
  std::unique_lock<std::mutex> lock(mutex);

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
    // Add new clients to list
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (!pending_clients.empty())
      {
        std::move(std::begin(pending_clients), std::end(pending_clients),
                  std::back_inserter(clients));
        pending_clients.clear();
      }
    }

    // Tick
    for (int i = 0; i < clients.size(); ++i)
    {
      ClientHandle * cl = clients.at(i).get();
      cl->tick();
    }

    // Clear disconnected clients
    for (auto it = clients.begin(); it != clients.end();)
    {
      if ((*it)->disconnected)
        it = clients.erase(it);
      else
        ++it;
    }

    // List events
    for (int i = 0; i < clients.size(); ++i)
    {
      ClientHandle * cl = clients.at(i).get();
      events[1 + i * 2] = cl->read_event;
      events[2 + i * 2] = cl->write_event;
    }

    // wake_event for thread + read & write per client
    int num_events = (int)(1 + clients.size() * 2);
    DWORD dw       = WaitForMultipleObjects(num_events, events, FALSE, INFINITE);

    // wake_event
    if (dw == WAIT_OBJECT_0)
    {
      // Event reset is handled by loop
      // Do nothing
    }
    // Read or write event
    else if (dw >= WAIT_OBJECT_0 && dw < WAIT_OBJECT_0 + num_events)
    {
      int client_num = (dw - WAIT_OBJECT_0 - 1) / 2;
      bool is_read   = ((dw - WAIT_OBJECT_0 - 1) % 2) == 0;

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
    // Error
    else
    {
      // TODO error
      assert(0 && "Windows wait error");
    }

    ResetEvent(wake_event);
  } // run loop

  // Close all clients on exit
  for (int i = 0; i < clients.size(); ++i)
    clients.at(i)->close();
}
// ClientThread run


WinAcceptThread::WinAcceptThread(mwrs_server_data * server) : server(server)
{
  // Do not call before all members are initialized
  std::thread t([this]() { run(); });
  thread.swap(t);
}

WinAcceptThread::~WinAcceptThread() { interrupt(); }

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
        pipename, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, pipeBufferSize, pipeBufferSize, 0, NULL);

    if (pipe == INVALID_HANDLE_VALUE)
    {
      // TODO ???
      assert(0 && "???");
      break;
    }

    ZeroMemory(&accept_overlapped, sizeof(accept_overlapped));

    ResetEvent(accept_event);
    accept_overlapped.hEvent = accept_event;

    if (ConnectNamedPipe(pipe, &accept_overlapped) != 0)
    {
      // TODO Critical error? should never happen
      assert(0 && "Should never happen");
      CloseHandle(pipe);
      continue;
    }

    DWORD err = GetLastError();

    if (err == ERROR_IO_PENDING)
    {
      HANDLE events[2]{wake_event, accept_event};

      // Ignore return,
      DWORD dw = WaitForMultipleObjects(2, events, FALSE, INFINITE);

      if (dw == WAIT_OBJECT_0)
      {
        // Wake
        err = ERROR_CANCELLED;
      }
      else if (dw == WAIT_OBJECT_0 + 1)
      {
        // Accept
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
      else
      {
        // TODO error
        assert(0 && "WaitForMultipleObjects error");
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
          client_threads.emplace_back(new WinClientThread(server));
          if (!client_threads.back()->try_add_client(pipe))
          {
            // TODO error
            assert(0 && "Failed to add client in new thread");
          }
        }
        catch (const std::exception &)
        {
          // TODO error
          assert(0 && "Failed to create new thread");
        }
      }
    }
    else
    {
      // TODO The client could not connect, so close the pipe.
      CloseHandle(pipe);
    }

    ResetEvent(wake_event);
  } // run loop

  // Close client threads
  for (auto & client_thread : client_threads)
    client_thread->interrupt();

  client_threads.clear();
}
// WinAcceptThread run


mwrs_ret fill_win_handle_from_res_open(const mwrs_client_data * client,
                                       const mwrs_sv_res_open * res_open,
                                       mwrs_sv_msg_common_response * response_out)
{
  HANDLE handle = INVALID_HANDLE_VALUE;

  switch (res_open->type)
  {
  case MWRS_SV_PATH:
    handle = CreateFileA(res_open->path,
                         ((response_out->open_flags & MWRS_OPEN_READ) ? GENERIC_READ : 0) |
                             ((response_out->open_flags & MWRS_OPEN_WRITE) ? GENERIC_WRITE : 0),
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
      return MWRS_E_NOTFOUND;
    break;
  case MWRS_SV_FD: handle = reinterpret_cast<HANDLE>(_get_osfhandle(res_open->fd)); break;
  case MWRS_SV_WIN_HANDLE: handle = res_open->win_handle; break;

  default: return MWRS_E_SERVERIMPL;
  }

  if (handle == INVALID_HANDLE_VALUE)
    return MWRS_E_SERVERIMPL;

  HANDLE duplicate = INVALID_HANDLE_VALUE;

  // DUPLICATE_CLOSE_SOURCE is not enough for FDs
  BOOL ok = DuplicateHandle(GetCurrentProcess(), handle, client->plat.handle->process, &duplicate,
                            0, TRUE, DUPLICATE_SAME_ACCESS);

  if (res_open->type == MWRS_SV_FD)
    _close(res_open->fd);
  else
    CloseHandle(handle);

  if (!ok)
  {
    // TODO error
    return MWRS_E_SERVERERR;
  }

  response_out->win_handle = to_mwrs_handle(duplicate);

  return MWRS_SUCCESS;
}
// fill_win_handle_from_res_open


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


void plat_client_queue_message(mwrs_client_data * client, mwrs_sv_message * message)
{
  client->plat.handle->queue_message(message);
}
// plat_client_queue_message

#endif // _WIN32


// Server instance holder
std::unique_ptr<mwrs_server_data> instance;


} // namespace


// API implementation

mwrs_ret mwrs_sv_init(const char * server_name, mwrs_sv_callbacks * callbacks)
{
  if (::instance)
    return MWRS_E_ALREADY;

  if (!server_name || !callbacks || !callbacks->open || !callbacks->stat)
    return MWRS_E_ARGS;

  ::instance.reset(new mwrs_server_data);

  // Must have null terminator
  std::memset(::instance->name, 0, MWRS_SERVER_NAME_MAX);
  std::strncpy(::instance->name, server_name, MWRS_SERVER_NAME_MAX - 1);
  ::instance->callbacks = *callbacks;

  mwrs_ret ret = plat_server_start(::instance.get());

  if (ret != MWRS_SUCCESS)
    ::instance.reset();

  return ret;
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
