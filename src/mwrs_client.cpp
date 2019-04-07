/**
 * @file    mwrs_client.cpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */

#include <mwrs.h>
#include "mwrs_messages.hpp"


#include <cassert>
#include <memory>
#include <mutex>

#ifdef _WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#endif


namespace
{


// Forward decl

struct mwrs_data;


// Platform-specific data

#ifdef _WIN32

struct mwrs_plat
{
  std::mutex mutex;
  HANDLE pipe = INVALID_HANDLE_VALUE;

  bool disconnected = false;
};

#endif // _WIN32

struct mwrs_data
{
  mwrs_plat plat;
};



// Platform-specific functions

mwrs_ret plat_start(mwrs_data * client, const char * server_name, int argc, const char ** argv);

void plat_stop(mwrs_data * client);

mwrs_ret plat_send_message(mwrs_data * client, const mwrs_cl_message * message);

mwrs_ret plat_receive_message(mwrs_data * client, mwrs_sv_message * message);


// Functions

mwrs_ret receive_response(mwrs_data * client, mwrs_sv_message * message)
{
  return plat_receive_message(client, message); // TODO
}

mwrs_ret send_res_request(mwrs_data* client, mwrs_cl_msg_type type, const char * res_id,
                          mwrs_open_flags flags = (mwrs_open_flags)0)
{
  mwrs_cl_message message;
  message.type = type;
  std::strncpy(message.resource_request.resource_id, res_id, MWRS_ID_MAX);
  message.resource_request.flags = flags;
  return plat_send_message(client, &message);
}

mwrs_ret common_response_get_res(const mwrs_sv_msg_common_response * response, mwrs_res * res_out)
{

}

mwrs_ret common_response_get_watcher(const mwrs_sv_msg_common_response * response, mwrs_watcher * watcher_out)
{

}

mwrs_ret common_response_get_status(const mwrs_sv_msg_common_response * response, mwrs_status * stat_out)
{

}



//



// Platform-specific implementation

#ifdef _WIN32

HANDLE to_win_handle(mwrs_win_handle_data mwrs_handle)
{
  static_assert(sizeof(mwrs_win_handle_data) == 4, "mwrs_win_handle_data must be 4 bytes");
  return reinterpret_cast<HANDLE>((unsigned long long)mwrs_handle);
}

mwrs_ret plat_start(mwrs_data * client, const char * server_name, int argc, const char ** argv)
{
  // Enough to hold "\\.\pipe\mwrs_" + server name + terminating null character
  TCHAR pipename[64 + MWRS_SERVER_NAME_MAX];
  _stprintf_s(pipename, 64 + MWRS_SERVER_NAME_MAX, TEXT("\\\\.\\pipe\\mwrs_%s"), server_name);

  client->plat.pipe = CreateFile(pipename,
    GENERIC_READ | GENERIC_WRITE,
    0, NULL, OPEN_EXISTING, 0, NULL);

  if (client->plat.pipe == INVALID_HANDLE_VALUE)
  {
    if (GetLastError() != ERROR_PIPE_BUSY)
      return MWRS_E_UNAVAIL; // TODO Check GetLastError for MWRS_E_SYSTEM

    // Wait 2s for pipe if busy
    if (!WaitNamedPipe(pipename, 2000))
      return MWRS_E_SYSTEM;

    // Retry
    client->plat.pipe = CreateFile(pipename,
      GENERIC_READ | GENERIC_WRITE,
      0, NULL, OPEN_EXISTING, 0, NULL);

    if (client->plat.pipe == INVALID_HANDLE_VALUE)
      return MWRS_E_UNAVAIL;
  }

  {
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(client->plat.pipe, &mode, NULL, NULL))
    {
      // TODO error
      CloseHandle(client->plat.pipe); // TODO in destructor instead
      assert(0 && "SetNamedPipeHandleState failed");
      return MWRS_E_SYSTEM;
    }
  }

  {
    // Send handshake
    mwrs_cl_message handshake;
    handshake.type = MWRS_MSG_CL_WIN_HANDSHAKE;
    handshake.win_handshake.mwrs_version = MWRS_VERSION;
    handshake.win_handshake.process_id = GetCurrentProcessId();

    mwrs_ret ret = plat_send_message(client, &handshake);

    if (ret != MWRS_SUCCESS)
    {
      CloseHandle(client->plat.pipe); // TODO in destructor instead
      return MWRS_E_SERVERERR;
    }
  }

  {
    // Receive ack
    mwrs_sv_message handshake_ack;
    mwrs_ret ret = plat_receive_message(client, &handshake_ack);

    if (ret != MWRS_SUCCESS
      || handshake_ack.type != MWRS_MSG_SV_WIN_HANDSHAKE_ACK
      || handshake_ack.win_handshake_ack.status != MWRS_SUCCESS)
    {
      CloseHandle(client->plat.pipe); // TODO in destructor instead

      if (ret == MWRS_SUCCESS && handshake_ack.type == MWRS_MSG_SV_WIN_HANDSHAKE_ACK)
        return handshake_ack.win_handshake_ack.status;
      else if (ret != MWRS_SUCCESS)
        return ret;
      else
        return MWRS_E_SERVERERR;
    }
  }

  return MWRS_SUCCESS;
}
// plat_start

void plat_stop(mwrs_data * client)
{
  CloseHandle(client->plat.pipe); // TODO in destructor instead
}
// plat_stop

mwrs_ret plat_send_message(mwrs_data * client, const mwrs_cl_message * message)
{
  if (client->plat.disconnected)
    return MWRS_E_BROKEN;

  DWORD written;
  if (!WriteFile(client->plat.pipe,(const void *) message, sizeof(mwrs_cl_message), &written, NULL))
  {
    if (GetLastError() == ERROR_BROKEN_PIPE)
    {
      client->plat.disconnected = true;
      return MWRS_E_BROKEN;
    }

    // TODO error
    return MWRS_E_SYSTEM;
  }

  if (written != sizeof(mwrs_cl_message))
  {
    // TODO error
    assert(0 && "Invalid write size");
    return MWRS_E_SYSTEM;
  }

  return MWRS_SUCCESS;
}
// plat_send_message

mwrs_ret plat_receive_message(mwrs_data * client, mwrs_sv_message * message)
{
  if (client->plat.disconnected)
    return MWRS_E_BROKEN;

  DWORD read;
  if (!ReadFile(client->plat.pipe, (void *)message, sizeof(mwrs_sv_message), &read, NULL))
  {
    if (GetLastError() == ERROR_BROKEN_PIPE)
    {
      client->plat.disconnected = true;
      return MWRS_E_BROKEN;
    }

    // TODO error
    return MWRS_E_SYSTEM;
  }

  if (read != sizeof(mwrs_sv_message))
  {
    // TODO error
    assert(0 && "Invalid read size");
    return MWRS_E_SYSTEM;
  }

  return MWRS_SUCCESS;
}

#endif // _WIN32



// Instance holder
std::unique_ptr<mwrs_data> instance;


}
// unnamed ns


// API implementation

int mwrs_res_is_valid(const mwrs_res * res)
{
  return res != (mwrs_res*)0;
}

int mwrs_watcher_is_valid(const mwrs_watcher * watcher)
{
  return watcher->id != 0;
}


mwrs_ret mwrs_init(const char * server_name, int argc, const char ** argv)
{
  if (::instance)
    return MWRS_E_ALREADY;

  if (!server_name || argc < 0)
    return MWRS_E_ARGS;

  ::instance.reset(new mwrs_data);

  mwrs_ret ret = plat_start(::instance.get(), server_name, argc, argv);

  if (ret != MWRS_SUCCESS)
    ::instance.reset();

  return ret;
}

mwrs_ret mwrs_shutdown()
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  plat_stop(::instance.get());
  ::instance.reset();

  return MWRS_SUCCESS;
}


mwrs_ret mwrs_open(const char * id, mwrs_open_flags flags, mwrs_res * res_out)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  if (mwrs_res_is_valid(res_out))
    return MWRS_E_ARGS;

  mwrs_ret ret;

  ret = send_res_request(::instance.get(), MWRS_MSG_CL_OPEN, id, flags);

  if (ret != MWRS_SUCCESS)
    return ret;

  mwrs_sv_message response;
  ret = receive_response(::instance.get(), &response);

  if (ret != MWRS_SUCCESS)
    return ret;

  if (response.type != MWRS_MSG_SV_COMMON_RESPONSE)
    return MWRS_E_PROTOCOL; // TODO kill client

  if (response.common_response.status == MWRS_SUCCESS)
  {
    ret = common_response_get_res(&response.common_response, res_out);

    if (ret != MWRS_SUCCESS)
      return MWRS_E_PROTOCOL; // TODO kill client
  }

  return response.common_response.status;
}

mwrs_ret mwrs_watcher_open(const mwrs_watcher* watcher, mwrs_open_flags flags, mwrs_res* res_out);

mwrs_ret mwrs_open_watch(const char * id, mwrs_open_flags flags, mwrs_res * res_out, mwrs_watcher * watcher_out)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  if (mwrs_res_is_valid(res_out))
    return MWRS_E_ARGS;

  if (mwrs_watcher_is_valid(watcher_out))
    return MWRS_E_ARGS;

  mwrs_ret ret;

  ret = send_res_request(::instance.get(), MWRS_MSG_CL_OPEN_WATCH, id, flags);

  if (ret != MWRS_SUCCESS)
    return ret;

  mwrs_sv_message response;
  ret = receive_response(::instance.get(), &response);

  if (ret != MWRS_SUCCESS)
    return ret;

  if (response.type != MWRS_MSG_SV_COMMON_RESPONSE)
    return MWRS_E_PROTOCOL; // TODO kill client

  if (response.common_response.status == MWRS_SUCCESS)
  {
    ret = common_response_get_res(&response.common_response, res_out);

    if (ret != MWRS_SUCCESS)
      return MWRS_E_PROTOCOL; // TODO kill client
  }

  // Ignore errors
  common_response_get_watcher(&response.common_response, watcher_out);

  return response.common_response.status;
}


mwrs_ret mwrs_stat(const char * id, mwrs_status * stat_out)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  mwrs_ret ret;

  ret = send_res_request(::instance.get(), MWRS_MSG_CL_STAT, id);

  if (ret != MWRS_SUCCESS)
    return ret;

  mwrs_sv_message response;
  ret = receive_response(::instance.get(), &response);

  if (ret != MWRS_SUCCESS)
    return ret;

  if (response.type != MWRS_MSG_SV_COMMON_RESPONSE)
    return MWRS_E_PROTOCOL; // TODO kill client

  if (response.common_response.status == MWRS_SUCCESS)
  {
    ret = common_response_get_status(&response.common_response, stat_out);

    if (ret != MWRS_SUCCESS)
      return MWRS_E_PROTOCOL; // TODO kill client
  }

  return response.common_response.status;
}

mwrs_ret mwrs_stat_watch(const char * id, mwrs_status * stat_out, mwrs_watcher * watcher_out)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  if (mwrs_watcher_is_valid(watcher_out))
    return MWRS_E_ARGS;

  mwrs_ret ret;

  ret = send_res_request(::instance.get(), MWRS_MSG_CL_STAT_WATCH, id);

  if (ret != MWRS_SUCCESS)
    return ret;

  mwrs_sv_message response;
  ret = receive_response(::instance.get(), &response);

  if (ret != MWRS_SUCCESS)
    return ret;

  if (response.type != MWRS_MSG_SV_COMMON_RESPONSE)
    return MWRS_E_PROTOCOL; // TODO kill client

  if (response.common_response.status == MWRS_SUCCESS)
  {
    ret = common_response_get_status(&response.common_response, stat_out);

    if (ret != MWRS_SUCCESS)
      return MWRS_E_PROTOCOL; // TODO kill client
  }

  // Ignore errors
  common_response_get_watcher(&response.common_response, watcher_out);

  return response.common_response.status;
}


mwrs_ret mwrs_watch(const char * id, mwrs_watcher * watcher_out)
{
  if (!::instance)
    return MWRS_E_UNAVAIL;

  if (mwrs_watcher_is_valid(watcher_out))
    return MWRS_E_ARGS;

  mwrs_ret ret;

  ret = send_res_request(::instance.get(), MWRS_MSG_CL_WATCH, id);

  if (ret != MWRS_SUCCESS)
    return ret;

  mwrs_sv_message response;
  ret = receive_response(::instance.get(), &response);

  if (ret != MWRS_SUCCESS)
    return ret;

  if (response.type != MWRS_MSG_SV_COMMON_RESPONSE)
    return MWRS_E_PROTOCOL; // TODO kill client

  if (response.common_response.status == MWRS_SUCCESS)
  {
    ret = common_response_get_watcher(&response.common_response, watcher_out);

    if (ret != MWRS_SUCCESS)
      return MWRS_E_PROTOCOL; // TODO kill client
  }

  return response.common_response.status;
}

mwrs_ret mwrs_close_watcher(mwrs_watcher* watcher);


/*mwrs_ret mwrs_read(mwrs_res * res, void * buffer, mwrs_size * read_len);

mwrs_ret mwrs_write(mwrs_res * res, const void * buffer, mwrs_size * write_len);

mwrs_ret mwrs_seek(mwrs_res * res, mwrs_size offset, mwrs_seek_origin origin);

mwrs_ret mwrs_tell(mwrs_res * res, mwrs_size * position_out);

mwrs_ret mwrs_close(mwrs_res * res);


mwrs_ret mwrs_poll_event(mwrs_event * event_out);

mwrs_ret mwrs_wait_event(mwrs_event * event_out);*/
