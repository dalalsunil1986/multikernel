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

#include <nanvix/runtime/pm.h>
#include "../test.h"

/**
 * @brief Process id for tests
 */
nanvix_pid_t pid;

/**
 * @brief API test: set pid and get pid
 */
static void test_proc_api_setpid(void)
{
	int source;

	TEST_ASSERT(nanvix_setpid() < 0);

	pid = nanvix_getpid();
	TEST_ASSERT(nanvix_pid_get_id(pid) > 0);
	
	source = nanvix_pid_get_source(pid);
	TEST_ASSERT(source == PROCESSOR_NODENUM_LEADER);
	TEST_ASSERT(source == knode_get_num());

	TEST_ASSERT(nanvix_getpid() == pid);
}

/**
 * @brief API test: set process group id
 */
static void test_proc_api_setpgid(void)
{
	pid = nanvix_getpid();

	TEST_ASSERT((nanvix_getpgid(pid) < (int)0));
	TEST_ASSERT(nanvix_getpgid(0) < 0);

	TEST_ASSERT(nanvix_setpgid(0, 0) == 0);
	TEST_ASSERT(nanvix_getpgid(pid) == (nanvix_gid_t)pid);
	TEST_ASSERT(nanvix_getpgid(0) == (nanvix_gid_t)pid);
}

/**
 * @brief API test: process lookup
 */
static void test_proc_api_lookup(void)
{
	pid = nanvix_getpid();

	TEST_ASSERT(nanvix_name_lookup2(pid) == knode_get_num());
}

/**
 * @brief Unit tests.
 */
struct test tests_proc_api[] = {
	{ test_proc_api_setpid,        "setpid"     },
	{ test_proc_api_setpgid,       "setpgid"    },
	{ test_proc_api_lookup,        "lookup"     },
	{ NULL,                        NULL         }
};
