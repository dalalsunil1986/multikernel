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
 * @brief Fault test: invalid pid.
 */
static void test_proc_fault_invalid_pid(void)
{
	nanvix_gid_t pgid;
	nanvix_pid_t pid;

	pid = nanvix_getpid();

	/* gid setted in api tests*/
	TEST_ASSERT((pgid = nanvix_getpgid(0)) > 0);

	pid = nanvix_pid_set_id(pid, -1);
	TEST_ASSERT(nanvix_setpgid(pid, 0) < 0);
	
	TEST_ASSERT(nanvix_setpgid(pid, pgid) < 0);
}

/**
 * @brief Fault test: invalid group id.
 */
static void test_proc_fault_invalid_gid(void)
{
	nanvix_pid_t pid;

	pid = nanvix_getpid();
	
	TEST_ASSERT(nanvix_setpgid(pid, -1) < 0);
	TEST_ASSERT(nanvix_setpgid(pid, 100) < 0);
	TEST_ASSERT(nanvix_setpgid(0, -1) < 0);
	TEST_ASSERT(nanvix_setpgid(0, 100) < 0);
}

/**
 * @brief Unit tests.
 */
struct test tests_proc_fault[] = {
	{ test_proc_fault_invalid_pid,        "invalid pid"      },
	{ test_proc_fault_invalid_gid,        "invalid group"    },
	{ NULL,                               NULL               }
};
