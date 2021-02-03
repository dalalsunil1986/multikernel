/*
 * MIT License
 *
 * Copyright(c) 2011-2020 The Maintainers of Nanvix
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef NANVIX_RUNTIME_PM_PROC_H_
#define NANVIX_RUNTIME_PM_PROC_H_

	#define __NEED_TYPES_PM
	#include <posix/stdint.h>
	#include <nanvix/types/pm.h>

	#define NANVIX_PID_NULL -1

	/**
	 * @name Nanvix pid fields length
	 */
	/**@{*/
	#define NANVIX_PID_ID_BITS       14 /**< Nanvix pid id length.       */
	#define NANVIX_PID_RESERVED_BITS 12 /**< Nanvix pid reserved length. */
	#define NANVIX_PID_SOURCE_BITS   6  /**< Nanvix pid source length.   */
	/**@}*/

	/**
	 * @name Nanvix pid fields offset
	 */
	/**@{*/
	#define NANVIX_PID_ID_OFFSET       0                  /**< Nanvix pid id offset.       */
	#define NANVIX_PID_RESERVED_OFFSET NANVIX_PID_ID_BITS /**< Nanvix pid reserved offset. */
	#define NANVIX_PID_SOURCE_OFFSET   \
		(NANVIX_PID_ID_BITS + NANVIX_PID_RESERVED_BITS)   /**< Nanvix pid source offset.   */
	/**@}*/

	/**
	 * @name Nanvix pid fields offset
	 */
	/**@{*/
	#define NANVIX_PID_ID_MASK       \
		(((unsigned)1 << NANVIX_PID_ID_BITS) - 1)       /**< Nanvix pid id mask.       */
	#define NANVIX_PID_RESERVED_MASK \
		(((unsigned)1 << NANVIX_PID_RESERVED_BITS) - 1) /**< Nanvix pid reserved mask. */
	#define NANVIX_PID_SOURCE_MASK   \
		(((unsigned)1 << NANVIX_PID_SOURCE_BITS) - 1)   /**< Nanvix pid source mask.   */
	/**@}*/

	/**
	 * @name Nanvix pid fields offset
	 */
	/**@{*/
	#define NANVIX_PID_ID_FIELD       \
		(NANVIX_PID_ID_MASK << NANVIX_PID_ID_OFFSET)             /**< Nanvix pid id field. */
	#define NANVIX_PID_RESERVED_FIELD \
		(NANVIX_PID_RESERVED_MASK << NANVIX_PID_RESERVED_OFFSET) /**< Nanvix pid reserved field. */
	#define NANVIX_PID_SOURCE_FIELD   \
		(NANVIX_PID_SOURCE_MASK << NANVIX_PID_SOURCE_OFFSET)     /**< Nanvix pid source field. */
	/**@}*/

	/**
	 * @brief Returns the id of a pid
	 */
	static inline unsigned nanvix_pid_get_id(nanvix_pid_t pid)
	{
		return (
			(((unsigned) pid) & NANVIX_PID_ID_FIELD) >>
			NANVIX_PID_ID_OFFSET
		);
	}

	/**
	 * @brief Returns the reserved of a pid
	 */
	static inline unsigned nanvix_pid_get_reserved(nanvix_pid_t pid)
	{
		return (
			(((unsigned) pid) & NANVIX_PID_RESERVED_FIELD) >>
			NANVIX_PID_RESERVED_OFFSET
		);
	}

	/**
	 * @brief Returns the source of a pid
	 */
	static inline unsigned nanvix_pid_get_source(nanvix_pid_t pid)
	{
		return (
			(((unsigned) pid) & NANVIX_PID_SOURCE_FIELD) >>
			NANVIX_PID_SOURCE_OFFSET
		);
	}

	/**
	 * @brief Changes @p pid id to @p id
	 */
	static inline nanvix_pid_t nanvix_pid_set_id(nanvix_pid_t pid, unsigned id)
	{
		return (
			(pid & ~NANVIX_PID_ID_FIELD) |
			((id & NANVIX_PID_ID_MASK) << NANVIX_PID_ID_OFFSET)
		);
	}

	/**
	 * @brief Changes @p pid reserved to @p reserved
	 */
	static inline nanvix_pid_t nanvix_pid_set_reserved(nanvix_pid_t pid, unsigned reserved)
	{
		return (
			(pid & ~NANVIX_PID_RESERVED_FIELD) | 
			((reserved & NANVIX_PID_RESERVED_MASK) << NANVIX_PID_RESERVED_OFFSET)
		);
	}

	/**
	 * @brief Changes @p pid source to @p source
	 */
	static inline nanvix_pid_t nanvix_pid_set_source(nanvix_pid_t pid, unsigned source)
	{
		return (
			(pid & ~NANVIX_PID_SOURCE_FIELD) |
			((source & NANVIX_PID_SOURCE_MASK) << NANVIX_PID_SOURCE_OFFSET)
		);
	}

	/**
	 * @brief Process info
	 */
	typedef struct
	{
		nanvix_pid_t pid;
		uint16_t nodenum;
	} nanvix_proc_info_t;

	/* Null proc info */
	#define NANVIX_PROC_INFO_NULL \
	(nanvix_proc_info_t) {        \
		.pid  = NANVIX_PID_NULL,  \
		.nodenum = -1             \
	}

	/**
	 * @brief Set process id
	 */
	extern int nanvix_setpid(void);

	/**
	 * @brief Returns process id
	 */
	extern nanvix_pid_t nanvix_getpid(void);

	/**
	 * @brief Returns process group id
	 */
	extern nanvix_gid_t nanvix_getpgid(nanvix_pid_t pid);

	/**
	 * @brief Set a process group id
	 *
	 * @param pid Target process id. If pid equals zero, the calling process id is used
	 * @param pgid Target process group id. If pid equals zero, a new group is
	 * created
	 *
	 * @return Upon successful completion, zero is returned. Upon
	 * failure, a negative error code is returned instead.
	 */
	extern int nanvix_setpgid(nanvix_pid_t pid, nanvix_gid_t pgid);

	/**
	 * @brief Converts a process id into a NoC node ID.
	 *
	 * @param pid Target pid.
	 *
	 * @returns Upon successful completion the NoC node ID whose name is @p
	 * pid is returned. Upon failure, a negative error code is returned
	 * instead.
	 */
	extern int nanvix_name_lookup2(nanvix_pid_t pid);

#endif /* NANVIX_RUNTIME_PM_PROC_H_ */
