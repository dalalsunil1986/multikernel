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

#include <nanvix/limits/pm.h>
#include <nanvix/runtime/stdikc.h>
#include <nanvix/servers/message.h>
#include <nanvix/servers/name.h>
#include <nanvix/sys/semaphore.h>
#include <nanvix/ulib.h>
#include <posix/errno.h>
#include <posix/stdint.h>

#define __DEBUG_NAME 0

#if (__DEBUG_NAME)
	static char debug_str[64];
	#define name_debug(fmt, ...) uprintf(fmt, __VA_ARGS__)
#else
	#define name_debug(fmt, ...) { }
#endif

/**
 * @brief Number of registration.
 */
static int nr_registration = 0;

/**
 * @brief Inbox.
 */
static int inbox = -1;

/**
 * @brief Process id counter
 */
static int pid_counter = 1;

/**
 * @brief Lookup table of process
 */
static struct {
	int nodenum;                     /**< NoC nodenum.              */
	uint64_t timestamp;              /**< Timestamp for heartbeats. */
	nanvix_pid_t pid;                /**< Process id.               */
	nanvix_gid_t gid;                /**< Process group id          */
} procs[NANVIX_PNAME_MAX];

/**
 * @brief Lookup table of process names
 */
static struct {
	char name[NANVIX_PROC_NAME_MAX]; /**< Process name.             */
	int port_nr;                     /**< Server connection.        */
	int refcount;                    /**< Link reference counter.   */
	int proc_index;                  /**< Index in process table    */
} names[NANVIX_PNAME_MAX];

/**
 * @brief Server stats.
 */
static struct
{
	int nlinks;         /**< Number of name link requests.   */
	int nunlinks;       /**< Number of unlink name requests. */
	int nlookups;       /**< Number of lookup requests.      */
} stats = { 0, 0, 0};

/*===================================================================*
 * do_name_init()                                                    *
 *===================================================================*/

/**
 * @brief Initializes the name server.
 */
static void do_name_init(struct nanvix_semaphore *lock)
{
	nanvix_pid_t pid;

	/* Initialize lookup table. */
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		procs[i].nodenum    = -1;
		procs[i].timestamp  =  0;
		procs[i].pid        =  NANVIX_PID_NULL;
		procs[i].gid        =  0;

		names[i].port_nr    = -1;
		names[i].proc_index = -1;
		names[i].refcount   =  0;
	}

	procs[0].nodenum  = knode_get_num();
	pid = NANVIX_PID_NULL;
	pid = nanvix_pid_set_source(pid, knode_get_num());
	pid = nanvix_pid_set_id(pid, pid_counter++);
	procs[0].pid = pid;

	ustrcpy(names[0].name, "/io0");
	names[0].port_nr    = kthread_self();
	names[0].refcount   = 1;
	names[0].proc_index = 0;

	uassert((inbox = stdinbox_get()) >= 0);

	/* Unblock spawner. */
	uprintf("[nanvix][name] server alive");
	uprintf("[nanvix][name] listening to mailbox %d", inbox);
	uprintf("[nanvix][name] attached to node %d", knode_get_num());

	nanvix_semaphore_up(lock);
}

/*=======================================================================*
 * do_name_lookup()                                                      *
 *=======================================================================*/

/**
 * @brief Converts a name into a NoC node number.
 *
 * @param requet   Request.
 * @param response Response.
 *
 * @returns Upon successful completion zero is returned. Upon failure, a
 * negative error code is returned instead.
 */
static int do_name_lookup(
	const struct name_message *request,
	struct name_message *response
)
{
	int ret;
	int pindex;
	nanvix_pid_t pid;
	const char *name;

	pid = request->op.lookup.pid;
	name = request->op.lookup.name;
	response->op.ret.proc_info = NANVIX_PROC_INFO_NULL;

	stats.nlookups++;
	name_debug("lookup name=%s", name);

	if (pid == NANVIX_PID_NULL)
	{
		/* Invalid name. */
		if ((ret = nanvix_name_is_valid(name)) < 0)
			return (ret);

		/* Search for portal name. */
		for (int i = 0; i < NANVIX_PNAME_MAX; i++)
		{
			/* Found. */
			if (!ustrcmp(name, names[i].name))
			{
				pindex = names[i].proc_index;
				response->op.ret.proc_info.nodenum = procs[pindex].nodenum;
				response->op.ret.proc_info.pid = procs[pindex].pid;

				return (0);
			}
		}
	}
	else
	{
		/* Search by pid */
		for (int i = 0; i < NANVIX_PNAME_MAX; i++)
		{
			/* Found. */
			if (procs[i].pid == pid)
			{
				response->op.ret.proc_info.nodenum = procs[i].nodenum;
				response->op.ret.proc_info.pid = procs[i].pid;

				return (0);
			}
		}
	}

	return (-ENOENT);
}

/*=======================================================================*
 * do_name_link()                                                        *
 *=======================================================================*/

/**
 * @brief Register a process name.
 *
 * @param request Request.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_name_link(const struct name_message *request)
{
	int ret;
	int index;
	int nindex;
	nanvix_pid_t pid;
	uint8_t remote_port;
	const char *name;

	name = request->op.link.name;
	pid = request->op.link.pid;
	remote_port = request->header.mailbox_port;

	stats.nlinks++;
	name_debug("link pid=%d name=%s", pid, name);

	/* Invalid name. */
	if ((ret = nanvix_name_is_valid(name)) < 0)
		return (ret);

	/* No entry available. */
	if (nr_registration >= NANVIX_PNAME_MAX)
		return (-EINVAL);

	/* Check if the name is already in use. */
	index = -1;
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		if (ustrcmp(names[i].name, name) == 0)
		{
			index = names[i].proc_index;

			/* Confirm connection. */
			if ((procs[index].pid == pid) && (names[i].port_nr == remote_port))
			{
				nindex = i;
				goto connect;
			}
			else
				return (-EINVAL);
		}

		if (procs[i].pid == pid)
			index = i;
	}

	if (index < 0)
		return (-EINVAL);
	
	/* Save new name. */
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		/* Available space. */
		if (names[i].proc_index < 0)
		{
			nindex = i;

			goto found;
		}
	}

	return (-EAGAIN);

found:
	ustrcpy(names[nindex].name, name);
	names[nindex].proc_index = index;
	names[nindex].port_nr = remote_port;

	nr_registration++;

connect:
	names[nindex].refcount++;

	return (0);
}

/*=======================================================================*
 * do_name_unlink()                                                      *
 *=======================================================================*/

/**
 * @brief Remove a name
 *
 * @param requet Request.
 *
 * @returns Upon successful completion zero is returned. Upon failure, a
 * negative error code is returned instead.
 */
static int do_name_unlink(const struct name_message *request)
{
	int ret;
	uint8_t remote_port;
	const char *name;

	name = request->op.unlink.name;
	remote_port = request->header.mailbox_port;

	stats.nlinks++;
	name_debug("unlink name=%s", name);

	/* Invalid name. */
	if ((ret = nanvix_name_is_valid(name)) < 0)
		return (ret);

	/* Search for name */
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		/* Skip invalid entries. */
		if (names[i].proc_index < 0)
			continue;

		/* Found*/
		if (ustrcmp(names[i].name, name) == 0)
		{
			/* Checks if it is the same port that linked this name. */
			if (names[i].port_nr != remote_port)
				return (-EINVAL);

			nr_registration--;

			/* Checks if it was the last reference to this name link. */
			if ((--names[i].refcount) == 0)
			{
				ustrcpy(names[i].name, "");
				names[i].proc_index = -1;
				names[i].port_nr    = -1;
			}

			return (0);
		}
	}

	return (-ENOENT);
}

/*=======================================================================*
 * do_name_heartbeat()                                                   *
 *=======================================================================*/

/**
 * @brief Updates the heartbeat of a process.
 *
 * @param request,
 *
 * @returns Upon successful completion zero is returned. Upon failure, a
 * negative error code is returned instead.
 */
static int do_name_heartbeat(const struct name_message *request)
{
	nanvix_pid_t pid;
	uint64_t timestamp;

	timestamp = request->op.heartbeat.timestamp;
	pid = request->header.pid;

	if (pid == NANVIX_PID_NULL)
		return (-EINVAL);

	name_debug("heartbeat pid=%d timestap=%l", pid, timestamp);

	/* Record timestamp. */
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		if (procs[i].pid == pid)
		{
			procs[i].timestamp = timestamp;
			return (0);
		}
	}

	return (-EINVAL);
}

/*===================================================================*
 * do_name_getpgid()                                                 *
 *===================================================================*/

/**
 * @brief Returns process group id
 */
static int do_name_getpgid(
	const struct name_message *request,
	struct name_message *response
)
{
	nanvix_pid_t pid;

	pid  = request->op.getpgid.pid;

	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		if (procs[i].pid == pid)
		{
			if (procs[i].gid == 0)
				break;

			response->op.ret.gid = procs[i].gid;
			return (0);
		}
	}

	return (-EAGAIN);
}

/*===================================================================*
 * do_name_setpgid()                                                     *
 *===================================================================*/

/**
 * @brief Set a process group id
 *
 * @return Upon successful completion, zero is returned. Upon
 * failure, a negative error code is returned instead.
 */
static int do_name_setpgid(const struct name_message *request)
{
	nanvix_pid_t pid;
	nanvix_gid_t pgid;

	pid = request->op.setpgid.pid;
	pgid = request->op.setpgid.pgid;

	if ((nanvix_gid_t) pid != pgid)
	{
		/* find group */
		for (int i = 0; i < NANVIX_PNAME_MAX; i++)
			if (procs[i].gid == pgid)
				goto set;
		
		return (-EPERM);
	}

set:
	/* set process group id*/
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		if (procs[i].pid == pid)
		{
			procs[i].gid = pgid;
			return (0);
		}
	}

	return (-ESRCH);
}

/*===================================================================*
 * do_name_setpid()                                                  *
 *===================================================================*/

/**
 * @brief Set process id
 */
static int do_name_setpid(
	const struct name_message *request,
	struct name_message *response
)
{
	nanvix_pid_t pid;
	int nodenum;

	nodenum = request->header.source;
	for (int i = 0; i < NANVIX_PNAME_MAX; i++)
	{
		if (procs[i].pid == NANVIX_PID_NULL)
		{
			pid = NANVIX_PID_NULL;
			pid = nanvix_pid_set_source(pid, nodenum);
			pid = nanvix_pid_set_id(pid, pid_counter++);
			procs[i].pid = pid;
			procs[i].nodenum = nodenum;

			response->op.ret.proc_info.pid = pid;
			response->op.ret.proc_info.nodenum = nodenum;

			return (0);
		}
	}

	return (-1);
}
/*===================================================================*
 * name_server()                                                     *
 *===================================================================*/

/**
 * @brief Handles remote name requests.
 *
 * @returns Always returns 0.
 */
int do_name_server(struct nanvix_semaphore *lock)
{
	int shutdown = 0;

	uprintf("[nanvix][name] booting up server");
	do_name_init(lock);

	while (!shutdown)
	{
		int outbox;
		int reply = 0;
		int ret = -ENOSYS;
		struct name_message request;
		struct name_message response;

		uassert(
			kmailbox_read(
				inbox,
				&request,
				sizeof(struct name_message)
			) == sizeof(struct name_message)
		);

		#if (__DEBUG_NAME)
		message_header_sprint(debug_str, &request.header);
		uprintf("name request %s", debug_str);
		#endif

		/* Handle name requests. */
		switch (request.header.opcode)
		{
			/* Lookup. */
			case NAME_LOOKUP:
				ret = do_name_lookup(&request, &response);
				reply = 1;
				break;

			/* Add name. */
			case NAME_LINK:
				ret = do_name_link(&request);
				reply = 1;
				break;

			/* Remove name. */
			case NAME_UNLINK:
				stats.nunlinks++;
				ret = do_name_unlink(&request);
				reply = 1;
				break;

			case NAME_ALIVE:
				uassert((ret = do_name_heartbeat(&request)) == 0);
				break;

			case NAME_GETPGID:
				ret = do_name_getpgid(&request, &response);
				reply = 1;
				break;

			case NAME_SETPGID:
				ret = do_name_setpgid(&request);
				reply = 1;
				break;

			case NAME_SETPID:
				ret = do_name_setpid(&request, &response);
				reply = 1;
				break;

			case NAME_EXIT:
				shutdown = 1;
				break;

			/* Should not happen. */
			default:
				break;
		}

		/* No reply? */
		if (!reply)
			continue;

		response.op.ret.errcode = ret;
		message_header_build(
			&response.header,
			(ret < 0) ? NAME_FAIL : NAME_SUCCESS
		);

		uassert((
			outbox = kmailbox_open(
				request.header.source,
				request.header.mailbox_port
			)) >= 0
		);
		uassert(
			kmailbox_write(
				outbox,
				&response,
				sizeof(struct name_message
			)) == sizeof(struct name_message)
		);
		uassert(kmailbox_close(outbox) == 0);
	}

	/* Dump statistics. */
	uprintf("[nanvix][name] links=%d lookups=%d unlinks=%d",
			stats.nlinks, stats.nlookups, stats.nunlinks
	);

	return (0);
}

/*============================================================================*
 * __main2()                                                                  *
 *============================================================================*/

/**
 * @brief Handles remote name requests.
 *
 * @returns Always returns zero.
 */
int name_server(struct nanvix_semaphore *lock)
{
	do_name_server(lock);

	return (0);
}
