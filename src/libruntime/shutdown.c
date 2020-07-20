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

#define __NEED_MM_RMEM_STUB
#define __NEED_SPAWN_SERVER

#include <nanvix/runtime/runtime.h>
#include <nanvix/servers/spawn.h>
#include <nanvix/ulib.h>

/**
 * The nanvix_shutdown() function shuts down sends a shutdown signal
 * to all system services, asking them to terminate.
 */
int nanvix_shutdown(void)
{
	/* Broadcast shutdown signal. */
	if (kcluster_get_num() == PROCESSOR_CLUSTERNUM_LEADER)
	{
		uprintf("[nanvix][%d] shutting down NOW",
			PROCESSOR_CLUSTERNUM_LEADER
		);
		__runtime_setup(SPAWN_RING_LAST);

		uassert(nanvix_vfs_shutdown() == 0);
		uassert(nanvix_rmem_shutdown() == 0);
		uassert(nanvix_sysv_shutdown() == 0);
		uassert(nanvix_name_shutdown() == 0);
	}

	return (0);
}
