/**
 * This file defines the C-only interface to interact with the d2log library.
 */

#ifndef D2_LOGGING_H
#define D2_LOGGING_H

#include <d2/detail/config.hpp>


#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


/**
 * Set the path of the repository into which events are written when logging
 * is enabled.
 *
 * The `path` must either:
 *  - Point to nothing (no file, no directory, etc..).
 *  - Point to an empty directory.
 * Anything else will make the call fail.
 *
 * @return Zero if the operation succeeded, and a non zero value otherwise.
 * @note This operation can be considered atomic.
 * @internal We might associate the return values to error codes in the future.
 */
D2_API extern int d2_set_log_repository(char const* path);

/**
 * Disable the logging of events by the library.
 *
 * @note This operation can be considered atomic.
 * @note This function is idempotent, i.e. calling it when the logging is
 *       already disabled is useless yet harmless.
 */
D2_API extern void d2_disable_event_logging(void);

/**
 * Enable the logging of events by the library.
 *
 * @note This operation can be considered atomic.
 * @note This function is idempotent, i.e. calling it when the logging is
 *       already enabled is useless yet harmless.
 */
D2_API extern void d2_enable_event_logging(void);

/**
 * Return 1 if event logging is currently enabled, and 0 otherwise.
 */
D2_API extern int d2_is_enabled(void);

/**
 * Return 1 if event logging is currently disabled, and 0 otherwise.
 */
D2_API extern int d2_is_disabled(void);

/**
 * Notify the library of the acquisition of a synchronization object with the
 * unique identitifer `lock_id` by the thread with the unique identitifer
 * `thread_id`.
 */
D2_API extern void d2_notify_acquire(size_t thread_id, size_t lock_id);

/**
 * Notify the library of the release of a synchronization object with the
 * unique identitifer `lock_id` by the thread with the unique identitifer
 * `thread_id`.
 */
D2_API extern void d2_notify_release(size_t thread_id, size_t lock_id);

/**
 * Notify the library of the start of a new thread uniquely identified by
 * `child_id` created by a thread uniquely identified by `parent_id`.
 */
D2_API extern void d2_notify_start(size_t parent_id, size_t child_id);

/**
 * Notify the library of the joining of a thread uniquely identified by
 * `child_id` into a thread uniquely identified by `parent_id`.
 */
D2_API extern void d2_notify_join(size_t parent_id, size_t child_id);

#ifdef __cplusplus
}
#endif

#endif /* !D2_LOGGING_H */