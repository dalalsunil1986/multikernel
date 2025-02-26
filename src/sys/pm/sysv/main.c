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
#define __SYSV_SERVER

#include <nanvix/servers/connection.h>
#include <nanvix/servers/sysv.h>
#include <nanvix/runtime/runtime.h>
#include <nanvix/sys/semaphore.h>
#include <nanvix/sys/mailbox.h>
#include <nanvix/sys/noc.h>
#include <nanvix/config.h>
#include <nanvix/ulib.h>

/**
 * @brief SYSV Server information.
 */
static struct
{
	int nodenum;  /**< Node number.  */
	int inbox;    /**< Input mailbox */
	int inportal; /**< Input Portal  */
	const char *name;
} server = {
	-1, -1, -1, SYSV_SERVER_NAME
};


/* Import definitions. */
extern void msg_test(void);
extern void sem_test(void);

/*============================================================================*
 * do_shm_open()                                                              *
 *============================================================================*/

/**
 * @brief Handles an open request.
 */
static int do_shm_open(struct sysv_message *request, struct sysv_message *response)
{
	int ret;

	ret = __do_shm_open(
		&response->payload.ret.page,
		request->header.source,
		request->payload.shm.open.name,
		request->payload.shm.open.oflags
	);

	if (ret < 0)
		return (ret);

	response->payload.ret.ipcid = ret;
	uassert(connect(request->header.source, request->header.mailbox_port) == 0);

	return (0);
}

/*============================================================================*
 * do_shm_close()                                                             *
 *============================================================================*/

/**
 * @brief Handles an close request.
 */
static int do_shm_close(struct sysv_message *request, struct sysv_message *response)
{
	int ret;

	ret = __do_shm_close(request->header.source, request->payload.shm.close.shmid);

	if (ret < 0)
		return (ret);

	response->payload.ret.status = ret;
	uassert(disconnect(
		request->header.source,
		request->header.mailbox_port
	) == 0);

	return (0);
}

/*============================================================================*
 * do_shm_create()                                                            *
 *============================================================================*/

/**
 * @brief Handles a create request.
 */
static int do_shm_create(struct sysv_message *request, struct sysv_message *response)
{
	int ret;

	ret = __do_shm_create(
		&response->payload.ret.page,
		request->header.source,
		request->payload.shm.create.name,
		request->payload.shm.create.oflags,
		request->payload.shm.create.mode
	);

	if (ret < 0)
		return (ret);

	response->payload.ret.ipcid = ret;
	uassert(connect(request->header.source, request->header.mailbox_port) == 0);

	return (0);
}

/*============================================================================*
 * do_shm_unlink()                                                            *
 *============================================================================*/

/**
 * @brief Handles an unlink request.
 */
static int do_shm_unlink(struct sysv_message *request, struct sysv_message *response)
{
	int ret;

	ret = __do_shm_unlink(request->header.source, request->payload.shm.unlink.name);

	if (ret < 0)
		return (ret);

	response->payload.ret.status = ret;

	return (0);
}

/*============================================================================*
 * do_shm_ftruncate()                                                         *
 *============================================================================*/

/**
 * @brief Handles an truncate request.
 */
static int do_shm_ftruncate(struct sysv_message *request, struct sysv_message *response)
{
	int ret;

	ret = __do_shm_ftruncate(
		&response->payload.ret.page,
		request->header.source,
		request->payload.shm.ftruncate.shmid,
		request->payload.shm.ftruncate.size
	);

	if (ret < 0)
		return (ret);

	response->payload.ret.status = ret;

	return (0);
}

/*============================================================================*
 * do_shm_inval()                                                             *
 *============================================================================*/

/**
 * @brief Handles an truncate request.
 */
static int do_shm_inval(struct sysv_message *request, struct sysv_message *response)
{
	int nremotes;
	int shmid;
	rpage_t page;
	struct connection remotes[NANVIX_PROC_MAX];

	shmid = request->payload.shm.inval.shmid;
	page = request->payload.shm.inval.page;

	sysv_debug("inval proc=%d shmid=%d page=%x",
		request->header.source,
		shmid,
		page
	);

	nremotes = get_connections(remotes);

	/* Broadcast invalidation signal. */
	for (int i = 0; i < nremotes; i++)
	{
		int outbox;
		struct sysv_message msg;

		message_header_build(
			&msg.header, SYSV_SHM_INVAL
		);

		msg.payload.shm.inval.shmid = shmid;
		msg.payload.shm.inval.page = page;

		uassert((
			outbox = kmailbox_open(
				remotes[i].remote,
				NANVIX_SHM_SNOOPER_PORT_NUM	
			)) >= 0
		);
		uassert(
			kmailbox_write(
				outbox,
				&msg,
				sizeof(struct sysv_message
			)) == sizeof(struct sysv_message)
		);
		uassert(kmailbox_close(outbox) == 0);
	}

	response->payload.ret.status = 0;

	return (0);
}

/*============================================================================*
 * do_sysv_msg_get()                                                          *
 *============================================================================*/

/**
 * @brief Handles a message queue get request.
 *
 * @param request  Target request.
 * @param response Response.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_msg_get(
	const struct sysv_message *request,
	struct sysv_message *response
)
{
	int ret;
	const int port = request->header.mailbox_port;
	const nanvix_pid_t pid = request->header.source;
	const int connection = connect(pid, port);

	((void) connection);

	ret = do_msg_get(
		request->payload.msg.get.key,
		request->payload.msg.get.msgflg
	);

	/* Operation failed. */
	if (ret < 0)
	{
		disconnect(pid, port);
		return (ret);
	}

	response->payload.ret.ipcid = ret;

	return (ret);
}

/*============================================================================*
 * do_sysv_msg_close()                                                        *
 *============================================================================*/

/**
 * @brief Handles a message queue close request.
 *
 * @param request  Target request.
 * @param response Response.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_msg_close(const struct sysv_message *request)
{
	int ret;
	const int port = request->header.mailbox_port;
	const nanvix_pid_t pid = request->header.source;

	ret = do_msg_close(request->payload.msg.close.msgid);

	/* Operation failed. */
	if (ret < 0)
		return (ret);

	disconnect(pid, port);

	return (ret);
}

/*============================================================================*
 * do_sysv_msg_send()                                                         *
 *============================================================================*/

/**
 * @brief Handles a message queue send request.
 *
 * @param request  Target request.
 * @param response Response.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_msg_send(const struct sysv_message *request)
{
	int ret;
	void *msgp;

	ret = do_msg_send(
		request->payload.msg.send.msgid,
		&msgp,
		request->payload.msg.send.msgsz,
		request->payload.msg.send.msgflg
	);

	/**
	 * Note that even though the operation may be failed, we guarantee
	 * the correct protocol. In the return message we inform wether or
	 * not the operation has succeeded.
	 */

	/* Allow remote write. */
	uassert(
		kportal_allow(
			server.inportal,
			request->header.source,
			request->header.portal_port
		) == 0
	);

	/* Read data in. */
	uassert(
		kportal_read(
			server.inportal,
			msgp,
			request->payload.msg.send.msgsz
		) == (ssize_t) request->payload.msg.send.msgsz
	);

	return (ret);
}

/*============================================================================*
 * do_sysv_msg_receive()                                                      *
 *============================================================================*/

/**
 * @brief Handles a message queue receive request.
 *
 * @param request  Target request.
 * @param response Response.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_msg_receive(const struct sysv_message *request)
{
	int ret;
	int outportal;
	int outbox;
	void *msgp;
	struct sysv_message msg;

	ret = do_msg_receive(
		request->payload.msg.receive.msgid,
		&msgp,
		request->payload.msg.receive.msgsz,
		request->payload.msg.receive.msgtyp,
		request->payload.msg.receive.msgflg
	);

	/* Operation failed. */
	if (ret < 0)
		return (ret);

	uassert((
		outbox = kmailbox_open(
			request->header.source,
			request->header.mailbox_port
		)) >= 0
	);

	/* Open portal to remote. */
	uassert((outportal =
		kportal_open(
			knode_get_num(),
			request->header.source,
			request->header.portal_port)
		) >= 0
	);

	/* Build operation header. */
	message_header_build2(
		&msg.header,
		SYSV_ACK,
		kcomm_get_port(outportal, COMM_TYPE_PORTAL)
	);

	/* Send acknowledge. */
	uassert(
		kmailbox_write(outbox,
			&msg,
			sizeof(struct sysv_message)
		) == sizeof(struct sysv_message)
	);

	/* Write to remote. */
	uassert(
		kportal_write(
			outportal,
			msgp,
			request->payload.msg.receive.msgsz
		) == (ssize_t) request->payload.msg.receive.msgsz
	);

	/* House keeping. */
	uassert(kportal_close(outportal) == 0);
	uassert(kmailbox_close(outbox) == 0);

	return (ret);
}

/*============================================================================*
 * do_sysv_msg_get()                                                          *
 *============================================================================*/

/**
 * @brief Handles a semaphore get request.
 *
 * @param request  Target request.
 * @param response Response.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_sem_get(
	const struct sysv_message *request,
	struct sysv_message *response
)
{
	int ret;
	const int port = request->header.mailbox_port;
	const nanvix_pid_t pid = request->header.source;
	const int connection = connect(pid, port);

	((void) connection);

	ret = do_sem_get(
		request->payload.sem.get.key,
		request->payload.sem.get.semflg
	);

	/* Operation failed. */
	if (ret < 0)
	{
		disconnect(pid, port);
		return (ret);
	}

	response->payload.ret.ipcid = ret;

	return (ret);
}

/*============================================================================*
 * do_sysv_sem_close()                                                        *
 *============================================================================*/

/**
 * @brief Handles a semaphore close request.
 *
 * @param request Target request.
 *
 * @returns Upon successful completion, zero is returned. Upon failure,
 * a negative error code is returned instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_sem_close(const struct sysv_message *request)
{
	int ret;
	const int port = request->header.mailbox_port;
	const nanvix_pid_t pid = request->header.source;

	ret = do_sem_close(request->payload.sem.close.semid);

	/* Operation failed. */
	if (ret < 0)
		return (ret);

	disconnect(pid, port);

	return (ret);
}

/*============================================================================*
 * do_sysv_sem_operate()                                                      *
 *============================================================================*/

/**
 * @brief Handles a semaphore operate request.
 *
 * @param request Target request.
 *
 * @returns Upon successful completion, either zero or one is returned.
 * Zero is returned when the process has completed the semaphore
 * operation, whereas one is returned when the process should block on
 * the semaphore.  Upon failure, a negative error code is returned
 * instead.
 *
 * @author Pedro Henrique Penna
 */
static int do_sysv_sem_operate(const struct sysv_message *request)
{
	int outbox;
	struct sysv_message response;
	const int port = request->header.mailbox_port;
	const nanvix_pid_t pid = request->header.source;
	int connection = connect(pid, port);

	connection = do_sem_operate(
		connection,
		request->payload.sem.operate.semid,
		&request->payload.sem.operate.sembuf
	);

	/* Operation failed. */
	if (connection < 0)
		return (connection);

	/* Block. */
	if (connection == pid)
		return (1);

	/*  Operation completed. */
	if (connection == 0)
		return (0);

	/* Unblock remote. */
	response.payload.ret.status = connection;
	message_header_build(
		&response.header,
		SYSV_SUCCESS
	);
	uassert((
		outbox = kmailbox_open(
			connection,
			connection_get_port(connection)
		)) >= 0
	);
	uassert(
		kmailbox_write(
			outbox,
			&response,
			sizeof(struct sysv_message
		)) == sizeof(struct sysv_message)
	);
	uassert(kmailbox_close(outbox) == 0);

	return (0);
}

/*============================================================================*
 * sysv_server_loop()                                                         *
 *============================================================================*/

/**
 * @brief Handles System V region requests.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_sysv_server_loop(void)
{
	int shutdown = 0;

	while (!shutdown)
	{
		int outbox;
		int reply = 0;
		int ret = -ENOSYS;
		struct sysv_message request;
		struct sysv_message response;

		uassert(
			kmailbox_read(
				server.inbox,
				&request,
				sizeof(struct sysv_message)
			) == sizeof(struct sysv_message)
		);

		sysv_debug("sysv request source=%d port=%d opcode=%d",
			request.header.source,
			request.header.mailbox_port,
			request.header.opcode
		);

		/* TODO check for bad node number. */

		/* Handle request. */
		switch (request.header.opcode)
		{
			case SYSV_SHM_CREATE:
				ret = do_shm_create(&request, &response);
				reply = 1;
				break;

			case SYSV_SHM_OPEN:
				ret = do_shm_open(&request, &response);
				reply = 1;
				break;

			case SYSV_SHM_UNLINK:
				ret = do_shm_unlink(&request, &response);
				reply = 1;
				break;

			case SYSV_SHM_CLOSE:
				ret = do_shm_close(&request, &response);
				reply = 1;
				break;

			case SYSV_SHM_FTRUNCATE:
				ret = do_shm_ftruncate(&request, &response);
				reply = 1;
				break;

			case SYSV_SHM_INVAL:
				ret = do_shm_inval(&request, &response);
				reply = 1;
				break;

			/* Get message queue. */
			case SYSV_MSG_GET:
				ret = do_sysv_msg_get(&request, &response);
				reply = 1;
				break;

			/* Close message queue. */
			case SYSV_MSG_CLOSE:
				ret = do_sysv_msg_close(&request);
				reply = 1;
				break;

			/* Send message. */
			case SYSV_MSG_SEND:
				ret = do_sysv_msg_send(&request);
				reply = 1;
				break;

			/* Receive message. */
			case SYSV_MSG_RECEIVE:
				ret = do_sysv_msg_receive(&request);
				reply = 1;
				break;

			/* Get semaphore. */
			case SYSV_SEM_GET:
				ret = do_sysv_sem_get(&request, &response);
				reply = 1;
				break;

			/* Close semaphore. */
			case SYSV_SEM_CLOSE:
				ret = do_sysv_sem_close(&request);
				reply = 1;
				break;

			/* Operate semaphore. */
			case SYSV_SEM_OPERATE:
				ret = do_sysv_sem_operate(&request);
				if (ret <= 0)
					reply = 1;
				break;

			/* Exit. */
			case SYSV_EXIT:
				shutdown = 1;
				break;

			default:
				break;
		}

		/* No reply? */
		if (!reply)
			continue;

		response.payload.ret.status = ret;
		message_header_build(
			&response.header,
			(ret < 0) ? SYSV_FAIL : SYSV_SUCCESS
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
				sizeof(struct sysv_message
			)) == sizeof(struct sysv_message)
		);
		uassert(kmailbox_close(outbox) == 0);
	}

#ifndef __SUPPRESS_TESTS
	uprintf("[nanvix][sysv] running self-tests...");
	msg_test();
	sem_test();
#endif

	return (0);
}

/*============================================================================*
 * do_sysv_server_startup()                                                    *
 *============================================================================*/

/**
 * @brief Initializes the System V server.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_sysv_server_startup(struct nanvix_semaphore *lock)
{
	int ret;

	uprintf("[nanvix][sysv] booting up server");

	server.nodenum = knode_get_num();
	server.inbox = stdinbox_get();
	server.inportal = stdinportal_get();

	/* Link name. */
	if ((ret = nanvix_name_link(server.nodenum, server.name)) < 0)
		return (ret);

	connections_setup();
	do_msg_init();
	do_sem_init();

	uprintf("[nanvix][sysv] minix System V created");

	uprintf("[nanvix][sysv] server alive");
	uprintf("[nanvix][sysv] attached to node %d", server.nodenum);
	uprintf("[nanvix][sysv] listening to mailbox %d", server.inbox);
	uprintf("[nanvix][sysv] listening to portal %d", server.inportal);

	nanvix_semaphore_up(lock);

	return (0);
}

/*============================================================================*
 * do_sysv_server_shutdown()                                                   *
 *============================================================================*/

/**
 * @brief Shutdowns the System V server.
 *
 * @returns Upon successful completion zero is returned. Upon failure, a
 * negative error code is returned instead.
 */
static int do_sysv_server_shutdown(void)
{
	uprintf("[nanvix][sysv] shutting down server");

	return (0);
}

/*============================================================================*
 * do_sysv_server()                                                           *
 *============================================================================*/

/**
 * @brief System V server.
 *
 * @returns Upon successful completion zero is returned. Upon failure, a
 * negative error code is returned instead.
 */
static int do_sysv_server(struct nanvix_semaphore *lock)
{
	int ret;

	if ((ret = do_sysv_server_startup(lock)) < 0)
	{
		uprintf("[nanvix][sysv] failed to startup server!");
		goto error;
	}

	if ((ret = do_sysv_server_loop()) < 0)
	{
		uprintf("[nanvix][sysv] failed to launch server!");
		goto error;
	}

	if ((ret = do_sysv_server_shutdown()) < 0)
	{
		uprintf("[nanvix][sysv] failed to shutdown server!");
		goto error;
	}

	return (0);

error:
	return (ret);
}

/*============================================================================*
 * sysv_server()                                                              *
 *============================================================================*/

/**
 * @brief Handles System V requests.
 *
 * @returns Always returns zero.
 */
int sysv_server(struct nanvix_semaphore *lock)
{
	uassert(do_sysv_server(lock) == 0);

	return (0);
}
