/**
 * @file    mwrs.h
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS__HEADER_GUARD
#define MWRS__HEADER_GUARD

#ifdef __cplusplus
extern "C" {
#endif

/* @MWRS_CMAKE_GENERATED@
#cmakedefine MWRS_SHARED
/* */

#ifdef MWRS_SHARED
#  if defined _WIN32 || defined __CYGWIN__
#    ifdef MWRS_SHARED_BUILD
#      define MWRS_API __declspec(dllexport)
#    else
#      define MWRS_API __declspec(dllimport)
#    endif
#  else
#    if __GNUC__ >= 4
#      define MWRS_API __attribute__((visibility("default")))
#    else
#      define MWRS_API
#    endif
#  endif
#else
#  define MWRS_API
#endif


// Constants
enum
{
  //               vvvv      Major
  //                   vvvv  Minor
  MWRS_VERSION = 0x00010000,

  // See sockaddr_un limitations
  // A byte is used for null terminator
  MWRS_SERVER_NAME_MAX = 64,
};


/**
 * File descriptor.
 */
typedef int mwrs_fd;


/**
 * Type for file positions, offsets and buffer sizes.
 *
 * A value of -1 means the size is not known.
 */
typedef long long int mwrs_size;


/**
 * Representation for Windows HANDLE.
 *
 * Windows 64 bits apps use 32 bits handles for compatibility
 * Only use lower bytes (truncated, not sign-extended)
 */
typedef unsigned int mwrs_win_handle_data;


/**
 * Status of a resource.
 */
typedef enum _mwrs_res_state
{
  MWRS_STATE_NOTFOUND = 1,
  MWRS_STATE_NOTREADY,
  MWRS_STATE_READY,

} mwrs_res_state;


/**
 * Status structure.
 */
typedef struct _mwrs_status
{
  mwrs_res_state state;
  mwrs_size size;
  int mtime;

} mwrs_status;


/**
 * Watcher identifier.
 */
typedef long long int mwrs_watcher_id;


/**
 * Return value for most functions
 */
typedef enum _mwrs_ret
{
  MWRS_SUCCESS = 0,

  /**
   * Input argument(s) are invalid.
   */
  MWRS_E_ARGS,

  /**
   * Server is unavailable.
   */
  MWRS_E_UNAVAIL,

  /**
   * Disconnected from peer.
   */
  MWRS_E_BROKEN,

  /**
   * Resource not found.
   */
  MWRS_E_NOTFOUND,

  /**
   * Resource exists but is not ready.
   */
  MWRS_E_NOTREADY,

  /**
   * Resource is not in an usable state.
   */
  MWRS_E_NOTOPEN,

  /**
   * Client version not supported by server.
   */
  MWRS_E_NOTSUPPORTED,

  /**
   * Access to resource not permitted (check open flags).
   */
  MWRS_E_PERM,

  /**
   * Operation refused.
   */
  MWRS_E_REFUSED,

  /**
   * Server-side error.
   */
  MWRS_E_SERVERERR,

  /**
   * Server implementation error. Check your callbacks.
   */
  MWRS_E_SERVERIMPL,

  /**
   * No data available right now, try again later.
   */
  MWRS_E_AGAIN,

  /**
   * System error.
   */
  MWRS_E_SYSTEM,

  /**
   * Protocol error.
   * The connection to the client must be shutdown.
   */
  MWRS_E_PROTOCOL,

  /**
   * Already initialized.
   */
  MWRS_E_ALREADY,

} mwrs_ret;


typedef enum _mwrs_open_flags
{

  MWRS_OPEN_READ   = 0x00000001,
  MWRS_OPEN_WRITE  = 0x00000002,
  MWRS_OPEN_APPEND = 0x00000004,
  MWRS_OPEN_SEEK   = 0x00000008,

  MWRS_OPEN_USER1 = 0x00010000,
  MWRS_OPEN_USER2 = 0x00020000,
  MWRS_OPEN_USER3 = 0x00040000,
  MWRS_OPEN_USER4 = 0x00080000,

} mwrs_open_flags;


typedef enum _mwrs_event_type
{
  /**
   * Resource is now available.
   */
  MWRS_EVENT_READY = 1,

  /**
   * Resource has been updated.
   */
  MWRS_EVENT_UPDATE,

  /**
   * Resource has been moved.
   */
  MWRS_EVENT_MOVE,

  /**
   * Resource has been deleted.
   */
  MWRS_EVENT_DELETE,


  MWRS_EVENT_USER1 = 0x100,
  MWRS_EVENT_USER2,
  MWRS_EVENT_USER3,
  MWRS_EVENT_USER4,

} mwrs_event_type;


typedef enum _mwrs_seek_origin
{
  MWRS_SEEK_SET = 1,
  MWRS_SEEK_CUR,
  MWRS_SEEK_END,

} mwrs_seek_origin;


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MWRS__HEADER_GUARD
