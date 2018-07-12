/*
 * MIT License
 *
 * Copyright (c) 2011-2018 Pedro Henrique Penna <pedrohenriquepenna@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.  THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>

#define __NEED_HAL_CORE_
#define __NEED_HAL_NOC_
#include <nanvix/hal.h>
#include <nanvix/init.h>

#include "test.h"

/**
 * @brief Number of cores in the underlying cluster.
 */
int ipc_barrier_ncores = 0;

/**
 * @brief Barrier Test Driver
 */
void test_ipc_barrier(int nbusycores)
{
	TEST_ASSERT(kernel_setup() == 0);

	ipc_barrier_ncores = hal_get_num_cores() - nbusycores;

	/* Run API tests. */
	for (int i = 0; ipc_barrier_tests_api[i].test_fn != NULL; i++)
	{
		printf("[nanvix][test][api][ipc][barrier] %s\n", ipc_barrier_tests_api[i].name);
		ipc_barrier_tests_api[i].test_fn();
	}

	TEST_ASSERT(kernel_cleanup() == 0);
}

