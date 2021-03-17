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

#include <nanvix/servers/name.h>
#include <nanvix/runtime/pm.h>
#include <nanvix/config.h>
#include <nanvix/ulib.h>
#include <nanvix/sys/task.h>
#include <posix/errno.h>
#include <posix/stdbool.h>

/**
 * @brief Mailbox for small messages.
 */
static int server;
static int server_heartbeat;
static int server_lookup;

/**
 * @brief Is the name service initialized ?
 */
static bool initialized = false;

static spinlock_t lock;

PRIVATE struct task_heartbeat
{
	ktask_t release;
	ktask_t * write;
	struct name_message msg;
	bool busy;
} task_heartbeat;

PRIVATE struct task_lookup
{
	ktask_t release;
	ktask_t * write;
	ktask_t * read;
	struct name_message msg;
	bool busy;
} task_lookup;

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

	/* Open connection with Name Server. */
	if ((server_heartbeat = kmailbox_open(NAME_SERVER_NODE, NAME_SERVER_PORT_NUM)) < 0)
		return (-1);

	/* Open connection with Name Server. */
	if ((server_lookup = kmailbox_open(NAME_SERVER_NODE, NAME_SERVER_PORT_NUM)) < 0)
		return (-1);

	KASSERT(server != server_heartbeat);
	KASSERT(server != server_lookup);
	KASSERT(server_heartbeat != server_lookup);

	task_heartbeat.busy = false;
	task_lookup.busy    = false;
	spinlock_init(&lock);

	initialized = true;

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

	return (0);
}

/*============================================================================*
 * nanvix_name_lookup()                                                       *
 *============================================================================*/

/**
 * @todo TODO: provide a long description for this function.
 */
int nanvix_name_lookup(const char *name)
{
	int ret;
	struct name_message msg;

	/* Initilize name client. */
	if (!initialized)
		return (-EAGAIN);

	/* Invalid name. */
	if ((ret = nanvix_name_is_valid(name)) < 0)
		return (ret);

	/* Build operation header. */
	message_header_build(&msg.header, NAME_LOOKUP);
	ustrcpy(msg.op.lookup.name, name);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	return (msg.op.ret.nodenum);
}

/*============================================================================*
 * nanvix_name_lookup()                                                       *
 *============================================================================*/

/**
 * @todo TODO: provide a long description for this function.
 */
PRIVATE int nanvix_name_lookup_release(ktask_args_t * args)
{
	if ((args->ret = kmailbox_task_release(task_lookup.write)) != 0)
		return (TASK_RET_ERROR);

	if ((args->ret = kmailbox_task_release(task_lookup.read)) != 0)
		return (TASK_RET_ERROR);

	spinlock_lock(&lock);
		task_lookup.busy = false;
	spinlock_unlock(&lock);

	return (TASK_RET_SUCCESS);
}

/**
 * @todo TODO: provide a detailed description for this function.
 */
ktask_t * nanvix_name_lookup_task_alloc(const char *name, int inbox, int port)
{
	int busy;
	struct name_message * msg = &task_lookup.msg;

	/* Initilize name client. */
	spinlock_lock(&lock);

		busy = !initialized || task_lookup.busy;

		if (!busy)
			task_lookup.busy = true;

	spinlock_unlock(&lock);

	if (busy)
		return (NULL);

	task_lookup.write = kmailbox_write_task_alloc(
		server_lookup,
		&task_lookup.msg,
		sizeof(struct name_message)
	);

	task_lookup.read = kmailbox_read_task_alloc(
		inbox, // stdinbox_get(),
		&task_lookup.msg,
		sizeof(struct name_message)
	);

	if (!task_lookup.write || !task_lookup.read)
	{
		kmailbox_task_release(task_lookup.write);
		kmailbox_task_release(task_lookup.read);

		return (NULL);
	}

	if (ktask_create(&task_lookup.release, nanvix_name_lookup_release, NULL, 0) != 0)
		return (NULL);

	if (ktask_connect(task_lookup.write, task_lookup.read) != 0)
		return (NULL);

	if (ktask_connect(task_lookup.read, &task_lookup.release) != 0)
		return (NULL);

	/* Build operation header. */
	message_header_build3(&msg->header, NAME_LOOKUP, port, port);
	ustrcpy(msg->op.lookup.name, name);

	KASSERT(ktask_dispatch(task_lookup.write) == 0);

	return (&task_lookup.release);
}

/*============================================================================*
 * nanvix_name_link()                                                         *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int nanvix_name_link(int nodenum, const char *name)
{
	int ret;
	struct name_message msg;

	/* Initilize name client. */
	if (!initialized)
		return (-EAGAIN);

	/* Invalid NoC node ID. */
	if (!proc_is_valid(nodenum))
		return (-EINVAL);

	/* Invalid name. */
	if ((ret = nanvix_name_is_valid(name)) < 0)
		return (ret);

	/* Build operation header. */
	message_header_build(&msg.header, NAME_LINK);
	ustrcpy(msg.op.link.name, name);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_SUCCESS)
		return (0);

	return (msg.op.ret.errcode);
}

/*============================================================================*
 * nanvix_name_unlink()                                                       *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int nanvix_name_unlink(const char *name)
{
	int ret;
	struct name_message msg;

	/* Initilize name client. */
	if (!initialized)
		return (-EAGAIN);

	/* Invalid name. */
	if ((ret = nanvix_name_is_valid(name)) < 0)
		return (ret);

	/* Build operation header. */
	message_header_build(&msg.header, NAME_UNLINK);
	ustrcpy(msg.op.unlink.name, name);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	if ((ret = kmailbox_read(stdinbox_get(), &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	if (msg.header.opcode == NAME_SUCCESS)
		return (0);

	return (msg.op.ret.errcode);
}

/*============================================================================*
 * nanvix_name_heartbeat()                                                    *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int nanvix_name_heartbeat(void)
{
	int ret;
	struct name_message msg;

	/* Initilize name client. */
	if (!initialized)
		return (-EAGAIN);

	/* Build operation header. */
	message_header_build(&msg.header, NAME_ALIVE);
	if ((ret = kernel_clock(&msg.op.heartbeat.timestamp)) < 0)
		return (ret);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	return (0);
}

/*============================================================================*
 * nanvix_name_heartbeat()                                                    *
 *============================================================================*/

PRIVATE int nanvix_name_heartbeat_release(ktask_args_t * args)
{
	if ((args->ret = kmailbox_task_release(task_heartbeat.write)) != 0)
		return (TASK_RET_ERROR);

	spinlock_lock(&lock);
		task_heartbeat.busy = false;
	spinlock_unlock(&lock);

	return (TASK_RET_SUCCESS);
}

/**
 * @todo TODO: provide a detailed description for this function.
 */
ktask_t * nanvix_name_heartbeat_task_alloc(void)
{
	int busy;
	struct name_message * msg = &task_heartbeat.msg;

	/* Initilize name client. */
	spinlock_lock(&lock);

		busy = !initialized || task_heartbeat.busy;

		if (!busy)
			task_heartbeat.busy = true;

	spinlock_unlock(&lock);

	if (busy)
		return (NULL);

	task_heartbeat.write = kmailbox_write_task_alloc(
		server_heartbeat,
		&task_heartbeat.msg,
		sizeof(struct name_message)
	);

	if (!task_heartbeat.write)
		return (NULL);

	if (ktask_create(&task_heartbeat.release, nanvix_name_heartbeat_release, NULL, 0) != 0)
		return (NULL);

	if (ktask_connect(task_heartbeat.write, &task_heartbeat.release) != 0)
		return (NULL);

	/* Build operation header. */
	message_header_build(&msg->header, NAME_ALIVE);
	if (kernel_clock(&msg->op.heartbeat.timestamp) < 0)
		return (NULL);

	KASSERT(ktask_dispatch(task_heartbeat.write) == 0);

	return (&task_heartbeat.release);
}

/*============================================================================*
 * nanvix_name_shutdown()                                                     *
 *============================================================================*/

/**
 * @todo TODO: provide a detailed description for this function.
 */
int nanvix_name_shutdown(void)
{
	int ret;
	struct name_message msg;

	/* Initilize name client. */
	if (!initialized)
		return (-EAGAIN);

	/* Build operation header. */
	message_header_build(&msg.header, NAME_EXIT);

	if ((ret = kmailbox_write(server, &msg, sizeof(struct name_message))) != sizeof(struct name_message))
		return (ret);

	return (0);
}
