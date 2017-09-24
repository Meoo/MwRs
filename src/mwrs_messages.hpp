/**
 * @file    mwrs_messages.hpp
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS_MESSAGES__HEADER_GUARD
#define MWRS_MESSAGES__HEADER_GUARD

#include <mwrs.h>

#include <stdint.h>

#pragma pack(push, 1)


enum mwrs_sv_msg_type
{

};


struct mwrs_sv_message
{
  mwrs_sv_msg_type type;
};


//


enum mwrs_cl_msg_type
{
  MWRS_MSG_CL_OPEN,
  MWRS_MSG_CL_WATCH,
  MWRS_MSG_CL_OPEN_WATCH,
  MWRS_MSG_CL_STAT,
  MWRS_MSG_CL_STAT_WATCH,
  MWRS_MSG_CL_CLOSE_WATCHER,

#ifdef WIN32
  MWRS_MSG_CL_WIN_HANDSHAKE,
  MWRS_MSG_CL_WIN_CONSUME_HANDLE,
#endif
};


struct mwrs_cl_msg_common
{
  char id[MWRS_ID_MAX];
  mwrs_open_flags flags; // used for open and open_watch
};

#ifdef WIN32
struct mwrs_cl_win_handshake
{
};

struct mwrs_cl_win_consume_handle
{
  uintptr_t handle;
};
#endif


struct mwrs_cl_message
{
  mwrs_cl_msg_type type;

  union
  {
    mwrs_cl_msg_common common;

#ifdef WIN32
    mwrs_cl_win_handshake win_handshake;
    mwrs_cl_win_consume_handle win_consume_handle;
#endif
  };
};


#pragma pack(pop)

#endif // MWRS_MESSAGES__HEADER_GUARD
