/**
 * @file    mwrs_messages.hpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS_MESSAGES__HEADER_GUARD
#define MWRS_MESSAGES__HEADER_GUARD

#include <mwrs.h>

#include <stdint.h>

#ifdef _WIN32
#  define VC_EXTRALEAN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#pragma pack(push, 1)
extern "C" {


enum mwrs_sv_msg_type
{
  MWRS_MSG_SV_COMMON_RESPONSE,

#ifdef _WIN32
  MWRS_MSG_SV_WIN_HANDSHAKE_ACK,
#endif
};


struct mwrs_sv_message
{
  mwrs_sv_msg_type type;
  unsigned int length;

  // data
};



struct mwrs_sv_msg_common_response
{
  mwrs_sv_msg_type type;
  unsigned int length;


  mwrs_ret status;

  // Resource
  mwrs_open_flags open_flags;
#ifdef _WIN32
  mwrs_win_handle_data win_handle;
#else
  mwrs_fd fd;
#endif

  // Stat
  mwrs_status stat;

  // Watcher
  mwrs_watcher_id watcher_id;
};

#ifdef _WIN32
struct mwrs_sv_win_handshake_ack
{
  mwrs_sv_msg_type type;
  unsigned int length;


  mwrs_ret status;
};
#endif


//


enum mwrs_cl_msg_type
{
  MWRS_MSG_CL_OPEN,
  MWRS_MSG_CL_WATCH,
  MWRS_MSG_CL_OPEN_WATCH,
  MWRS_MSG_CL_STAT,
  MWRS_MSG_CL_STAT_WATCH,

  MWRS_MSG_CL_WATCHER_OPEN,
  MWRS_MSG_CL_CLOSE_WATCHER,

#ifdef _WIN32
  MWRS_MSG_CL_WIN_HANDSHAKE,
#endif
};


struct mwrs_cl_message
{
  mwrs_cl_msg_type type;
  unsigned int length;

  // data
};


struct mwrs_cl_msg_resource_request
{
  mwrs_cl_msg_type type;
  unsigned int length;


  mwrs_open_flags flags; // used for open and open_watch

  char resource_id; // extend message
};

struct mwrs_cl_msg_watcher_request
{
  mwrs_cl_msg_type type;
  unsigned int length;


  mwrs_watcher_id watcher_id;

  mwrs_open_flags flags; // used for open
};

#ifdef _WIN32
struct mwrs_cl_win_handshake
{
  mwrs_cl_msg_type type;
  unsigned int length;


  int mwrs_version;
  DWORD process_id;

  int argc;
  char argv; // extend message
};
#endif


} // extern "C"
#pragma pack(pop)

#endif // MWRS_MESSAGES__HEADER_GUARD
