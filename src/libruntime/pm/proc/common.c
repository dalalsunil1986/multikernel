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

/* Must come first. */
#define __NEED_NAME_SERVER
#define __NEED_NAME_SERVICE

#include <nanvix/runtime/pm.h>
#include <nanvix/runtime/pm/proc.h>
#include <nanvix/servers/name.h>
#include <nanvix/config.h>
#include <nanvix/ulib.h>
#include <posix/stdbool.h>
#include "common.h"

/**
 * @brief Mailbox for small messages.
 */
int server;

/**
 * @brief Is the name service initialized ?
 */
bool initialized = false;

/**
 * @brief Process info
 */
nanvix_proc_info_t proc_info;

/*============================================================================*
 * __nanvix_name_setup()                                                      *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int __nanvix_name_setup(void)
{
	/* Nothing to do. */
	if (initialized)
		return (0);

	/* Open connection with Name Server. */
	if ((server = kmailbox_open(NAME_SERVER_NODE, NAME_SERVER_PORT_NUM)) < 0)
		return (-1);

	initialized = true;
	proc_info = NANVIX_PROC_INFO_NULL;

	uassert(nanvix_setpid() == 0);

	return (0);
}

/*============================================================================*
 * nanvix_name_cleanup()                                                      *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int __nanvix_name_cleanup(void)
{
	/* Nothing to do. */
	if (!initialized)
		return (0);

	/* Close connection with Name Server. */
	if (kmailbox_close(server) < 0)
		return (-EAGAIN);

	initialized = false;
	proc_info = NANVIX_PROC_INFO_NULL;

	return (0);
}

/*============================================================================*
 * __nanvix_name_lookup()                                                     *
 *============================================================================*/

/**
 * @brief Gets process info of a process by name or pid
 *
 * @param name Target name.
 * @param pid  Target pid.
 * @param p    Target process info.
 *
 * @returns Upon successful completion the NoC node ID whose name is @p
 * name or pid is @p pid is returned. Upon failure, a negative error code is returned
 * instead.
 */
int __nanvix_name_lookup (
	nanvix_pid_t pid,
	const char *name,
	nanvix_proc_info_t *p
)
{
	struct name_message msg;
	int ret;

	/* Not initialized */
	if (!initialized)
		return (-EINVAL);

	/* Build message */
	message_header_build(&msg.header, NAME_LOOKUP);
	msg.op.lookup.pid = pid;
	if (pid == NANVIX_PID_NULL)
		ustrcpy(msg.op.lookup.name, name);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_FAIL)
		return (msg.op.ret.errcode);

	umemcpy(p, &msg.op.ret.proc_info, sizeof(nanvix_proc_info_t));

	return (0);
}
