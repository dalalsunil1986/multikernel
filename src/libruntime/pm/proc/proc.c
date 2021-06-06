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
#define __NEED_LIMITS_PM
#define __NEED_NAME_SERVER
#define __NEED_NAME_SERVICE

#include <nanvix/limits/pm.h>
#include <nanvix/runtime/pm.h>
#include <nanvix/servers/name.h>
#include <nanvix/ulib.h>
#include <posix/errno.h>
#include "common.h"

#define NANVIX_PID_RESERVED 0 /* Reserved pid value */

/**
 * @brief Name of the running process.
 */
static char _pname[NANVIX_PROC_NAME_MAX] = "";

/*============================================================================*
 * get_pname ()                                                               *
 *============================================================================*/

/**
 * The nanvix_getpname() function returns the name of the calling
 * process.
 */
const char *nanvix_getpname(void)
{
	return (_pname);
}

/*============================================================================*
 * set_pname ()                                                               *
 *============================================================================*/

/**
 * The nanvix_setpname() function sets the name of the calling process
 * to @pname.
 */
int nanvix_setpname(const char *pname)
{
	int ret;
	nanvix_pid_t pid;

	/* Invalid name. */
	if (ustrlen(pname) >= NANVIX_PROC_NAME_MAX)
		return (-EINVAL);

	if (ustrcmp(_pname, ""))
		return (-EBUSY);

	if ((pid = nanvix_getpid()) == NANVIX_PID_NULL)
		return (-1);

	/* Link process name. */
	if ((ret = nanvix_name_link(pid, pname)) < 0)
		return (ret);

	ustrcpy(_pname, pname);

	return (0);
}

/*============================================================================*
 * nanvix_setpid()                                                            *
 *============================================================================*/

/**
 * @brief Set process id
 */
int nanvix_setpid(void)
{
	struct name_message msg;
	nanvix_pid_t pid;
	int ret;

	/* Not initialized. */
	if (!initialized)
		return (-EINVAL);

	if ((pid = nanvix_getpid()) != NANVIX_PID_NULL)
		return (-EPERM);

	message_header_build(&msg.header, NAME_SETPID);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_FAIL)
		return (msg.op.ret.errcode);

	proc_info = msg.op.ret.proc_info;
	
	return (0);
}

/*============================================================================*
 * nanvix_getpid ()                                                           *
 *============================================================================*/

/**
 * @brief Returns process id
 */
inline nanvix_pid_t nanvix_getpid(void)
{
	return (proc_info.pid);
}

/*============================================================================*
 * nanvix_get_proc_nodenum ()                                                 *
 *============================================================================*/

/**
 * @brief Returns process nodenum
 */
inline int nanvix_proc_get_nodenum(void)
{
	return (proc_info.nodenum);
}

/*============================================================================*
 * nanvix_getpgid()                                                           *
 *============================================================================*/

/**
 * @brief Returns process group id
 * 
 * @param pid Target pid.
 */
nanvix_gid_t nanvix_getpgid(nanvix_pid_t pid)
{
	struct name_message msg;
	nanvix_pid_t local_pid;
	int ret;

	if (!initialized)
		return (-EINVAL);

	if (pid == NANVIX_PID_NULL)
		return (-EINVAL);

	message_header_build(&msg.header, NAME_GETPGID);
	
	/* If pid equals zero, the calling process pid is used */
	if (pid == NANVIX_PID_RESERVED)
	{
		if ((local_pid = nanvix_getpid()) == NANVIX_PID_NULL)
			return (-ESRCH);

		/* Use calling process pid */
		msg.op.getpgid.pid = local_pid;
	}
	else
		msg.op.getpgid.pid = pid;

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_FAIL)
		return (msg.op.ret.errcode);
	
	return (msg.op.ret.gid);
}

/*============================================================================*
 * nanvix_setpgid()                                                           *
 *============================================================================*/

/**
 * @brief Set a process group id
 *
 * @param pid Target process id. If pid equals zero, the calling process id is used
 * @param pgid Target process group id. If pgid equals zero, a new group is
 * created
 *
 * @return Upon successful completion, zero is returned. Upon
 * failure, a negative error code is returned instead.
 */
int nanvix_setpgid(nanvix_pid_t pid, nanvix_gid_t pgid)
{
	struct name_message msg;
	nanvix_pid_t local_pid;
	int ret;

	if (!initialized)
		return (-EINVAL);

	/* If pid equals zero, the calling process pid is used */
	if (pid == NANVIX_PID_RESERVED)
	{
		if ((local_pid = nanvix_getpid()) == NANVIX_PID_NULL)
			return (-ESRCH);

		/* Use calling process pid */
		pid = local_pid;
	}

	/* If pgid equals zero, a new group is created */
	if (pgid == NANVIX_PID_RESERVED)
		pgid = (nanvix_gid_t) pid;

	/* Build message */
	message_header_build(&msg.header, NAME_SETPGID);
	msg.op.setpgid.pid = pid;
	msg.op.setpgid.pgid = pgid;

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message)))
		!= sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_FAIL)
		return (msg.op.ret.errcode);
	
	return (0);
}

/*============================================================================*
 * nanvix_name_lookup2()                                                     *
 *============================================================================*/

/**
 * @brief Converts a process id into a NoC node ID.
 *
 * @param pid Target pid.
 *
 * @returns Upon successful completion the NoC node ID whose name is @p
 * pid is returned. Upon failure, a negative error code is returned
 * instead.
 * 
 * @see __nanvix_name_lookup()
 */
int nanvix_name_lookup2(const nanvix_pid_t pid)
{
	nanvix_proc_info_t p;
	int ret;

	/* Not initialized */
	if (!initialized)
		return (-EINVAL);

	/* Invelid pid */
	if (pid == NANVIX_PID_NULL)
		return(-EINVAL);
	
	if ((ret = __nanvix_name_lookup(pid, NULL, &p)) < 0)
		return (ret);

	return (p.nodenum);
}
