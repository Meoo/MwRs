/**
 * @file    mwrs_client.cpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */

#include <mwrs.h>


#include <atomic>
#include <thread>

#ifdef WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <tchar.h>
#endif


namespace
{

struct mwrs_server
{
  char name[MWRS_SERVER_NAME_MAX + 1];

  mwrs_sv_callbacks callbacks;
};


#ifdef WIN32

// "Multithreaded Pipe Server"
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365588(v=vs.85).aspx

struct mwrs_sv_client_win
{
  mwrs_sv_client client;

  HANDLE pipe;
};


// https://stackoverflow.com/questions/35828965/named-pipes-server-how-to-interrupt-or-timeout-the-wait-for-client-connection-a
void win_wait_event(HANDLE event)
{
  DWORD dw;
  MSG msg;

  for (;;)
  {
    dw = MsgWaitForMultipleObjectsEx(1, &event, INFINITE, QS_ALLINPUT, 0);

    if (dw == WAIT_OBJECT_0)
      break;

    if (dw == WAIT_OBJECT_0 + 1)
    {
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessage(&msg);
      continue;
    }

    // TODO error GetLastError()
  }
}
// win_wait_event


void client_thread(mwrs_server * server, mwrs_sv_client_win * client)
{
  // Manual reset event
  HANDLE read_event = CreateEvent(NULL, true, true, NULL);
  OVERLAPPED overlapped;

  for (;;)
  {
    ZeroMemory(&overlapped, sizeof(overlapped));

    ResetEvent(read_event);
    overlapped.hEvent = read_event;

    DWORD err = ERROR_SUCCESS;

    if (ReadFile(client->pipe, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, &overlapped) == 0)
      err = GetLastError();

    if (err == ERROR_IO_PENDING)
    {
      win_wait_event(read_event);

      DWORD unused;
      if (GetOverlappedResult(client->pipe, &overlapped, &unused, FALSE))
        err = ERROR_SUCCESS;
      else
        err = GetLastError();
    }

    if (err == ERROR_SUCCESS)
    {
      // TODO Received
    }
    else
    {
      // TODO Error
    }
  }
}
// client_thread


void accept_thread(mwrs_server * server)
{
  // Enough to hold "\\.\pipe\mwrs_" + server name + terminating null character
  TCHAR pipename[64 + MWRS_SERVER_NAME_MAX];
  _stprintf_s(pipename, 64 + MWRS_SERVER_NAME_MAX, TEXT("\\\\.\\pipe\\mwrs_%s"), server.name);

  // Manual reset event
  HANDLE accept_event = CreateEvent(NULL, true, true, NULL);
  OVERLAPPED overlapped;

  for (;;)
  {
    HANDLE pipe = CreateNamedPipe(
      pipename,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES,
      BUFSIZE,
      BUFSIZE,
      0,
      NULL);

    if (pipe == INVALID_HANDLE_VALUE)
    {
      // TODO ???
      break;
    }

    ZeroMemory(&overlapped, sizeof(overlapped));

    ResetEvent(accept_event);
    overlapped.hEvent = accept_event;

    if (ConnectNamedPipe(pipe, &overlapped) != 0)
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
      if (GetOverlappedResult(pipe, &overlapped, &unused, FALSE))
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
      // TODO Connected
    }
    else
    {
      // TODO The client could not connect, so close the pipe.
      CloseHandle(pipe);
    }
  }

  CloseHandle(accept_event);
}
// accept_thread

#endif // WIN32

}
// unnamed ns
