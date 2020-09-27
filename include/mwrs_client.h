/**
 * @file    mwrs_client.h
 * @author  Bastien Brunnenstein
 * @license BSD 3-Clause
 */


#ifndef MWRS_CLIENT__HEADER_GUARD
#define MWRS_CLIENT__HEADER_GUARD

#include "mwrs.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Resource handle.
 * Should be zero initialized before use.
 */
typedef struct _mwrs_res
{
  mwrs_open_flags flags;
  void * opaque;

} mwrs_res;


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


mwrs_ret MWRS_API mwrs_move(const char * id_from, const char * id_to);

// TODO remove
mwrs_ret MWRS_API mwrs_delete(const char * id);


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


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MWRS_CLIENT__HEADER_GUARD
