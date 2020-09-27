/**
 * @file    mwrs_server.h
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS_SERVER__HEADER_GUARD
#define MWRS_SERVER__HEADER_GUARD

#include "mwrs.h"

#ifdef __cplusplus
extern "C" {
#endif


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


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MWRS_SERVER__HEADER_GUARD
