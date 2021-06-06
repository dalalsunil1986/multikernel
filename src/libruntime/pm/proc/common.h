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


#include <nanvix/runtime/pm/proc.h>

	/**
	 * @brief Mailbox for small messages.
	 */
	extern int server;

	/**
	 * @brief Is the name service initialized ?
	 */
	extern bool initialized;

	/**
	 * @brief Process info
	 */
	extern nanvix_proc_info_t proc_info;
	
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
	extern int __nanvix_name_lookup(nanvix_pid_t pid, const char *name, nanvix_proc_info_t *proc_info);
