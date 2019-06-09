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


//
//  SHARED HEADER
//


// Constants
enum
{
  //               vvvv      Major
  //                   vvvv  Minor
  MWRS_VERSION = 0x00010000,

  // See sockaddr_un limitations
  // A byte is used for null terminator
  MWRS_SERVER_NAME_MAX = 64,

  // Max length for resource identifier
  // A byte is used for null terminator
  MWRS_ID_MAX = 512
};


/**
 * File descriptor.
 */
typedef int mwrs_fd;


/**
 * Type for file positions, offsets and buffer sizes.
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


//
//  CLIENT HEADER
//


/**
 * Resource identifier.
 */
typedef char mwrs_res_id[MWRS_ID_MAX];

/**
 * Resource handle.
 * Should be zero initialized before use.
 */
typedef struct _mwrs_res
{
  mwrs_open_flags flags;
  void * opaque;

} mwrs_res;


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
 * Watcher handle.
 * Should be zero initialized before use.
 */
typedef struct _mwrs_watcher
{
  mwrs_watcher_id id;

} mwrs_watcher;


/**
 * Watcher event.
 */
typedef struct _mwrs_event
{
  mwrs_watcher_id watcher_id;
  mwrs_event_type type;

} mwrs_event;


/**
 * Returns 1 if the resource handle is valid, 0 otherwise.
 */
int MWRS_API mwrs_res_is_valid(const mwrs_res * res);

/**
 * Returns 1 if the watcher handle is valid, 0 otherwise.
 */
int MWRS_API mwrs_watcher_is_valid(const mwrs_watcher * watcher);


/**
 * Open a pipe to the local server named `server_name`.
 */
mwrs_ret MWRS_API mwrs_init(const char * server_name, int argc, const char ** argv);

/**
 * Close the connection with the server.
 *
 * All the remaining handles are invalidated,
 * and using them in any way is undefined behaviour.
 */
mwrs_ret MWRS_API mwrs_shutdown();


/**
 * Open a resource.
 */
mwrs_ret MWRS_API mwrs_open(const char * id, mwrs_open_flags flags, mwrs_res * res_out);

/**
 * Open a resource pointed by a valid watcher.
 */
mwrs_ret MWRS_API mwrs_watcher_open(const mwrs_watcher * watcher, mwrs_open_flags flags,
                                    mwrs_res * res_out);

/**
 * Simultaneously open and watch a resource.
 *
 * This function will try to watch the resource even in case of error.
 * You can use `mwrs_watcher_is_valid` to check if the watcher has been created.
 * It is always safe to call `mwrs_close_watcher` on the watcher after this function.
 * If the resource has been opened, the watcher will not produce a READY event,
 * otherwise the behaviour is the same as `mwrs_watch`.
 */
mwrs_ret MWRS_API mwrs_open_watch(const char * id, mwrs_open_flags flags, mwrs_res * res_out,
                                  mwrs_watcher * watcher_out);


mwrs_ret MWRS_API mwrs_stat(const char * id, mwrs_status * stat_out);

/**
 * Simultaneously stat and watch a resource.
 *
 * This function will try to watch the resource even in case of error.
 * You can use `mwrs_watcher_is_valid` to check if the watcher has been created.
 * It is always safe to call `mwrs_close_watcher` on the watcher after this function.
 * If the stat is successful, the watcher will not produce a READY event,
 * otherwise the behaviour is the same as `mwrs_watch`.
 */
mwrs_ret MWRS_API mwrs_stat_watch(const char * id, mwrs_status * stat_out,
                                  mwrs_watcher * watcher_out);


/**
 * Open a watcher to a resource.
 *
 * If the resource is available, a READY event will be produced.
 */
mwrs_ret MWRS_API mwrs_watch(const char * id, mwrs_watcher * watcher_out);

/**
 * Close a watcher.
 *
 * Pending events for this watcher will not be received.
 */
mwrs_ret MWRS_API mwrs_close_watcher(mwrs_watcher * watcher);


mwrs_ret MWRS_API mwrs_read(mwrs_res * res, void * buffer, mwrs_size * read_len);

mwrs_ret MWRS_API mwrs_write(mwrs_res * res, const void * buffer, mwrs_size * write_len);

mwrs_ret MWRS_API mwrs_seek(mwrs_res * res, mwrs_size offset, mwrs_seek_origin origin,
                            mwrs_size * position_out);

mwrs_ret MWRS_API mwrs_close(mwrs_res * res);


/**
 * Get next event, non-blocking.
 *
 * Returns E_AGAIN if no event is available.
 */
mwrs_ret MWRS_API mwrs_poll_event(mwrs_event * event_out);

/**
 * Get next event, blocking.
 */
mwrs_ret MWRS_API mwrs_wait_event(mwrs_event * event_out);


//
//  SERVER HEADER
//


#ifdef MWRS_INCLUDE_SERVER


typedef struct _mwrs_sv_client
{
  int id;
  void * userdata;

} mwrs_sv_client;


typedef enum _mwrs_sv_file_type
{
  MWRS_SV_PATH = 1,
  MWRS_SV_FD,

  /// Only supported on Windows
  MWRS_SV_WIN_HANDLE,

} mwrs_sv_file_type;

typedef struct _mwrs_sv_res_open
{
  mwrs_sv_file_type type;

  union
  {
    const char * path;
    int fd;
    void * win_handle;
  };

} mwrs_sv_res_open;


/**
 * Callback for client connection.
 *
 * Callbacks can be invoked from any thread.
 *
 * If you need the client object to carry custom data, you can use `client.userdata`.
 *
 * Return `MWRS_SUCCESS` to accept the connection.
 * Every other code will deny the connection, disconnect callback will not be called.
 */
typedef mwrs_ret (*mwrs_sv_callback_connect)(mwrs_sv_client * client, int argc, const char ** argv);

/**
 * Callback for client disconnection.
 *
 * Callbacks can be invoked from any thread.
 *
 * If you set a custom `client.userdata`, clean it here.
 */
typedef void (*mwrs_sv_callback_disconnect)(mwrs_sv_client * client);


/**
 * Callback for open function.
 *
 * Callbacks can be invoked from any thread.
 *
 * `open_out` is already allocated and initialized, just fill the structure.
 * Only fill it if you return `MWRS_SUCCESS`.
 * If you give a file descriptor or a Windows handle, you don't have to close it manually.
 */
typedef mwrs_ret (*mwrs_sv_callback_open)(mwrs_sv_client * client, const char * id,
                                          mwrs_open_flags flags, mwrs_sv_res_open * open_out);

/**
 * Callback for stat function.
 *
 * Callbacks can be invoked from any thread.
 *
 * `stat_out` is already allocated and initialized, just fill the structure.
 * Only fill it if you return `MWRS_SUCCESS`.
 */
typedef mwrs_ret (*mwrs_sv_callback_stat)(mwrs_sv_client * client, const char * id,
                                          mwrs_status * stat_out);

/**
 * Callback when a resource starts being watched.
 *
 * Callbacks can be invoked from any thread.
 *
 * This callback is invoked when the first watcher for `id` is added.
 */
typedef mwrs_ret (*mwrs_sv_callback_watch)(const char * id);

/**
 * Callback when a resource stops being watched.
 *
 * Callbacks can be invoked from any thread.
 *
 * This callback is invoked when the last watcher for `id` is removed.
 */
typedef mwrs_ret (*mwrs_sv_callback_unwatch)(const char * id);


typedef struct _mwrs_sv_callbacks
{
  mwrs_sv_callback_connect connect;
  mwrs_sv_callback_disconnect disconnect;

  mwrs_sv_callback_open open;
  mwrs_sv_callback_stat stat;

  mwrs_sv_callback_watch watch;
  mwrs_sv_callback_unwatch unwatch;

} mwrs_sv_callbacks;


mwrs_ret MWRS_API mwrs_sv_init(const char * server_name, mwrs_sv_callbacks * callbacks);

mwrs_ret MWRS_API mwrs_sv_shutdown();


/**
 * Push an event to all connected clients listening to a resource.
 */
mwrs_ret MWRS_API mwrs_sv_push_event(const char * id, mwrs_event_type type);


#endif // MWRS_INCLUDE_SERVER


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MWRS__HEADER_GUARD
