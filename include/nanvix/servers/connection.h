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

#ifndef SERVERS_CONNECTION_H_
#define SERVERS_CONNECTION_H_

	#include <nanvix/limits.h>
	#include <nanvix/types.h>

	/**
	 * @brief Connection
	 */
	struct connection
	{
		nanvix_pid_t remote; /**< PID             */
		int port;            /**< Port Number     */
		int count;           /**< Reference Count */
	};

	/**
	 * @brief Initializes the table of connections.
	 */
	extern void connections_setup(void);

	/**
	 * @brief Searches for a registered connection.
	 *
	 * @param pid  PID of the remote.
	 * @param port Remote port number.
	 *
	 * @returns If the target remote is found, its index in the table of
	 * connections is returned. Otherwise a negative number is returned
	 * instead.
	 */
	extern int lookup(nanvix_pid_t remote, int port);

	/**
	 * @brief Establishes a connection
	 *
	 * @param pid  PID of the remote.
	 * @param port Remote port number.
	 *
	 * @returns Upon sucessful completion, zero is returned. Upon failure, a
	 * negative error code is returned instead.
	 */
	extern int connect(nanvix_pid_t remote, int port);

	/**
	 * @brief Gets port of remote connection.
	 *
	 * @param connection Target connection.
	 *
	 * @returns Upon sucessful completion, the port number of the remote
	 * connection is returned. Upon failure, a negative error code is
	 * returned instead.
	 */
	extern int connection_get_port(int connection);

	/**
	 * @brief Unlinks a connection
	 *
	 * @param pid  PID of the remote.
	 * @param port Remote port number.
	 *
	 * @returns Upon sucessful completion, zero is returned. Upon failure, a
	 * negative error code is returned instead.
	 */
	extern int disconnect(nanvix_pid_t remote, int port);

	/**
	 * @brief Gets active connections.
	 *
	 * @param buf Buffer to store info on connections.
	 *
	 * @returns Upon successful completion, the number of connections
	 * placed in @p buf is returned. Upon failure, a negative error code
	 * is returned instead.
	 */
	extern int get_connections(struct connection *buf);

#endif /* SERVERS_CONNECTION_H_ */
