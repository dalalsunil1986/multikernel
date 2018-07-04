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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mppaipc.h>

#define __NEED_HAL_CORE_
#define __NEED_HAL_NOC_
#define __NEED_HAL_SETUP_
#define __NEED_HAL_SYNC_
#include <nanvix/const.h>
#include <nanvix/hal.h>

#include "test.h"

/*============================================================================*
 * API Test: Create Unlink                                                    *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Create Unlink
 */
static void *test_hal_sync_create_unlink_worker(void *args)
{
	int tnum;
	int syncid;

	hal_setup();

	tnum = ((int *)args)[0];

	nodes[tnum] = hal_get_node_id();

	/*
	 * Wait for nodes list to be initialized. 
	 */
	pthread_barrier_wait(&barrier);

	TEST_ASSERT((syncid = hal_sync_create(nodes, ncores, HAL_SYNC_ONE_TO_ALL)) >= 0);

	/*
	 * Wait for all processes to create the 
	 * their synchronization points.
	 */
	pthread_barrier_wait(&barrier);

	TEST_ASSERT(hal_sync_unlink(syncid) == 0);

	hal_cleanup();
	return (NULL);
}

/**
 * @brief API Test: Synchronization Point Create Unlink
 */
static void test_hal_sync_create_unlink(void)
{
	int args[ncores];
	pthread_t tids[ncores];

	nodes[0] = hal_get_node_id();

	/* Spawn driver threads. */
	for (int i = 1; i < ncores; i++)
	{
		args[i] = i;
		assert((pthread_create(&tids[i],
			NULL,
			test_hal_sync_create_unlink_worker,
			&args[i])) == 0
		);
	}

	/* Wait for driver threads. */
	for (int i = 1; i < ncores; i++)
		pthread_join(tids[i], NULL);
}

/*============================================================================*
 * API Test: Open Close                                                       *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Open Close
 */
static void *test_hal_sync_open_close_worker(void *args)
{
	int tnum;
	int syncid;

	hal_setup();

	tnum = ((int *)args)[0];

	nodes[tnum] = hal_get_node_id();

	/*
	 * Wait for nodes list to be initialized. 
	 */
	pthread_barrier_wait(&barrier);

	TEST_ASSERT((syncid = hal_sync_create(nodes, ncores, HAL_SYNC_ONE_TO_ALL)) >= 0);

	/*
	 * Wait for all processes to open the 
	 * their synchronization points.
	 */
	pthread_barrier_wait(&barrier);

	TEST_ASSERT(hal_sync_unlink(syncid) == 0);

	hal_cleanup();
	return (NULL);
}

/**
 * @brief API Test: Synchronization Point Open Close
 */
static void test_hal_sync_open_close(void)
{
	int args[ncores];
	pthread_t tids[ncores];

	nodes[0] = hal_get_node_id();

	/* Spawn driver threads. */
	for (int i = 1; i < ncores; i++)
	{
		args[i] = i;
		assert((pthread_create(&tids[i],
			NULL,
			test_hal_sync_open_close_worker,
			&args[i])) == 0
		);
	}

	/* Wait for driver threads. */
	for (int i = 1; i < ncores; i++)
		pthread_join(tids[i], NULL);
}

/*============================================================================*
 * API Test: Wait Signal                                                      *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Wait Signal
 */
static void *test_hal_sync_wait_signal_worker(void *args)
{
	int tnum;
	int syncid;

	hal_setup();

	tnum = ((int *)args)[0];

	nodes[tnum] = hal_get_node_id();

	/*
	 * Wait for nodes list to be initialized. 
	 */
	pthread_barrier_wait(&barrier);

	if (tnum == 0)
	{
		TEST_ASSERT((syncid = hal_sync_open(&nodes[0], ncores - 1, HAL_SYNC_ONE_TO_ALL)) >= 0);

		/*
		 * Wait for all processes to open the 
		 * their synchronization points.
		 */
		pthread_barrier_wait(&barrier);

		TEST_ASSERT(hal_sync_signal(syncid) == 0);

		TEST_ASSERT(hal_sync_close(syncid) == 0);
	}
	else
	{
		TEST_ASSERT((syncid = hal_sync_create(&nodes[0], ncores - 1, HAL_SYNC_ONE_TO_ALL)) >= 0);

		/*
		 * Wait for all processes to open the 
		 * their synchronization points.
		 */
		pthread_barrier_wait(&barrier);

		TEST_ASSERT(hal_sync_wait(syncid) == 0);

		TEST_ASSERT(hal_sync_unlink(syncid) == 0);
	}

	hal_cleanup();
	return (NULL);
}

/**
 * @brief API Test: Synchronization Point Wait Signal
 */
static void test_hal_sync_wait_signal(void)
{
	int args[ncores];
	pthread_t tids[ncores];

	nodes[0] = hal_get_node_id();

	/* Spawn driver threads. */
	for (int i = 1; i < ncores; i++)
	{
		args[i] = i - 1;
		assert((pthread_create(&tids[i],
			NULL,
			test_hal_sync_wait_signal_worker,
			&args[i])) == 0
		);
	}

	/* Wait for driver threads. */
	for (int i = 1; i < ncores; i++)
		pthread_join(tids[i], NULL);
}

/*============================================================================*
 * API Test: Signal Wait                                                      *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Signal Wait
 */
static void *test_hal_sync_signal_wait_worker(void *args)
{
	int tnum;
	int syncid;

	hal_setup();

	tnum = ((int *)args)[0];

	nodes[tnum] = hal_get_node_id();

	/*
	 * Wait for nodes list to be initialized. 
	 */
	pthread_barrier_wait(&barrier);

	if (tnum == 0)
	{
		TEST_ASSERT((syncid = hal_sync_create(&nodes[0], ncores - 1, HAL_SYNC_ALL_TO_ONE)) >= 0);

		/*
		 * Wait for all processes to open the 
		 * their synchronization points.
		 */
		pthread_barrier_wait(&barrier);

		TEST_ASSERT(hal_sync_wait(syncid) == 0);

		TEST_ASSERT(hal_sync_unlink(syncid) == 0);
	}
	else
	{
		TEST_ASSERT((syncid = hal_sync_open(&nodes[0], ncores - 1, HAL_SYNC_ALL_TO_ONE)) >= 0);

		/*
		 * Wait for all processes to open the 
		 * their synchronization points.
		 */
		pthread_barrier_wait(&barrier);

		TEST_ASSERT(hal_sync_signal(syncid) == 0);

		TEST_ASSERT(hal_sync_close(syncid) == 0);
	}

	hal_cleanup();
	return (NULL);
}

/**
 * @brief API Test: Synchronization Point Signal Wait
 */
static void test_hal_sync_signal_wait(void)
{
	int args[ncores];
	pthread_t tids[ncores];

	/* Build nodes list. */
	for (int i = 0; i < ncores; i++)
		nodes[i] = hal_get_node_id() + i;

	/* Spawn driver threads. */
	for (int i = 1; i < ncores; i++)
	{
		args[i] = i - 1;
		assert((pthread_create(&tids[i],
			NULL,
			test_hal_sync_signal_wait_worker,
			&args[i])) == 0
		);
	}

	/* Wait for driver threads. */
	for (int i = 1; i < ncores; i++)
		pthread_join(tids[i], NULL);
}

/*============================================================================*
 * API Test: Barrier Mode                                                     *
 *============================================================================*/

/**
 * @brief API Test: Barrier Mode
 */
static void test_hal_sync_barrier(void)
{
	int nodeid;
	int syncid;
	int syncid_local;
	int _nodes[2];
	int _nodes_local[2];

	nodeid = hal_get_node_id();

	_nodes[0] = nodeid;
	_nodes[1] = hal_noc_nodes[SPAWNER1_SERVER_NODE];

	_nodes_local[0] = hal_noc_nodes[SPAWNER1_SERVER_NODE];
	_nodes_local[1] = nodeid;

	/* Open synchronization points. */
	TEST_ASSERT((syncid_local = hal_sync_create(_nodes_local, 2, HAL_SYNC_ONE_TO_ALL)) >= 0);
	TEST_ASSERT((syncid = hal_sync_open(_nodes, 2, HAL_SYNC_ONE_TO_ALL)) >= 0);

	TEST_ASSERT(hal_sync_signal(syncid) == 0);
	TEST_ASSERT(hal_sync_wait(syncid_local) == 0);

	/* House keeping. */
	TEST_ASSERT(hal_sync_unlink(syncid_local) == 0);
	TEST_ASSERT(hal_sync_close(syncid) == 0);
}

/*============================================================================*
 * API Test: Compute Clusters tests                                           *
 *============================================================================*/

/**
 * @brief API Test: Compute Clusters tests
 */
static void test_hal_sync_create_unlink_cc(void)
{
	int status;
	int pids[sync_nclusters];

	char sync_nclusters_str[4];
	char test_str[4];
	const char *args[] = {
		"/test/hal-sync-slave",
		sync_nclusters_str,
		test_str,
		NULL
	};

	sprintf(sync_nclusters_str, "%d", sync_nclusters);
	sprintf(test_str, "%d", 0);

	for (int i = 0; i < sync_nclusters; i++)
		TEST_ASSERT((pids[i] = mppa_spawn(i, NULL, args[0], args, NULL)) != -1);

	for (int i = 0; i < sync_nclusters; i++)
	{
		TEST_ASSERT(mppa_waitpid(pids[i], &status, 0) != -1);
		TEST_ASSERT(status == EXIT_SUCCESS);
	}
}

/*============================================================================*
 * API Test: Wait Signal IO-Compute cluster                                   *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Wait Signal
 */
static void test_hal_sync_wait_signal_cc(void)
{
	int syncid;
	int _nodes[sync_nclusters + 1];
	int pids[sync_nclusters];
	int status;

	char sync_nclusters_str[4];
	char test_str[4];
	const char *args[] = {
		"/test/hal-sync-slave",
		sync_nclusters_str,
		test_str,
		NULL
	};

	/* Build nodes list. */
	_nodes[0] = hal_get_node_id();

	for (int i = 0; i < sync_nclusters; i++)
		_nodes[i + 1] = i;

	sprintf(sync_nclusters_str, "%d", sync_nclusters);
	sprintf(test_str, "%d", 1);

	for (int i = 0; i < sync_nclusters; i++)
		TEST_ASSERT((pids[i] = mppa_spawn(i, NULL, args[0], args, NULL)) != -1);

	TEST_ASSERT((syncid = hal_sync_open(_nodes, sync_nclusters + 1, HAL_SYNC_ONE_TO_ALL)) >= 0);

	TEST_ASSERT(hal_sync_signal(syncid) == 0);

	TEST_ASSERT(hal_sync_close(syncid) == 0);

	for (int i = 0; i < sync_nclusters; i++)
	{
		TEST_ASSERT(mppa_waitpid(pids[i], &status, 0) != -1);
		TEST_ASSERT(status == EXIT_SUCCESS);
	}
}

/*============================================================================*
 * API Test: Signal Wait IO-Compute cluster                                   *
 *============================================================================*/

/**
 * @brief API Test: Synchronization Point Wait Signal
 */
static void test_hal_sync_signal_wait_cc(void)
{
	int syncid;
	int _nodes[sync_nclusters + 1];
	int pids[sync_nclusters];
	char sync_nclusters_str[4];
	char test_str[4];
	const char *args[] = {
		"/test/hal-sync-slave",
		sync_nclusters_str,
		test_str,
		NULL
	};

	/* Build nodes list. */
	_nodes[0] = hal_get_node_id();

	for (int i = 0; i < sync_nclusters; i++)
		_nodes[i + 1] = i;

	/* Create synchronization point. */
	TEST_ASSERT((syncid = hal_sync_create(
		_nodes,
		sync_nclusters + 1,
		HAL_SYNC_ALL_TO_ONE)) >= 0
	);

	/* Spawn slaves. */
	sprintf(sync_nclusters_str, "%d", sync_nclusters);
	sprintf(test_str, "%d", 2);
	for (int i = 0; i < sync_nclusters; i++)
		TEST_ASSERT((pids[i] = mppa_spawn(i, NULL, args[0], args, NULL)) != -1);

	/* Wait. */
	TEST_ASSERT(hal_sync_wait(syncid) == 0);

	/* Join. */
	for (int i = 0; i < sync_nclusters; i++)
	{
		int status;

		TEST_ASSERT(mppa_waitpid(pids[i], &status, 0) != -1);
		TEST_ASSERT(status == EXIT_SUCCESS);
	}

	/* House keeping. */
	TEST_ASSERT(hal_sync_unlink(syncid) == 0);
}

/*============================================================================*/

/**
 * @brief Unit tests.
 */
struct test tests_api[] = {
	/* Intra-Cluster API Tests */
	{ test_hal_sync_create_unlink,    "Create Unlink" },
	{ test_hal_sync_open_close,       "Open Close"    },
	{ test_hal_sync_wait_signal,      "Wait Signal"   },
	{ test_hal_sync_signal_wait,      "Signal Wait"   },
	{ test_hal_sync_barrier,          "Barrier Mode"  },
	{ NULL,                           NULL            },
	/* Inter-Cluster API Tests*/
	{ test_hal_sync_create_unlink_cc, "CClusters Open Close"   },
	{ test_hal_sync_wait_signal_cc,   "IOClusters -> CCluster" },
	{ test_hal_sync_signal_wait_cc,   "CCluster -> IO Cluster" },
	{ NULL,                           NULL                     },
};
