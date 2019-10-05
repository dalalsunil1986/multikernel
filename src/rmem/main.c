/*
 * MIT License
 *
 * Copyright(c) 2011-2019 The Maintainers of Nanvix
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

#define __RMEM_SERVICE
#define __NEED_NAME_CLIENT

#include <nanvix/servers/message.h>
#include <nanvix/servers/name.h>
#include <nanvix/servers/rmem.h>
#include <nanvix/servers/spawn.h>
#include <nanvix/runtime/stdikc.h>
#include <nanvix/runtime/runtime.h>
#include <nanvix/runtime/utils.h>
#include <nanvix/sys/mailbox.h>
#include <nanvix/sys/noc.h>
#include <nanvix/sys/perf.h>
#include <nanvix/sys/portal.h>
#include <nanvix/limits.h>
#include <ulibc/assert.h>
#include <ulibc/stdio.h>
#include <ulibc/stdlib.h>
#include <ulibc/string.h>
#include <posix/errno.h>
#include <stdint.h>

/**
 * @brief Debug RMEM?
 */
#define __DEBUG_RMEM 0

#if (__DEBUG_RMEM)
	#define rmem_debug(fmt, ...) debug("rmem", fmt, __VA_ARGS__)
#else
	#define rmem_debug(fmt, ...) { }
#endif

/**
 * @brief Server statistics.
 */
static struct
{
	unsigned nallocs;   /**< Number of allocations. */
	unsigned nfrees;    /**< Number of frees.       */
	unsigned nreads;    /**< Number of reads.       */
	unsigned nwrites;   /**< Number of writes.      */
	uint64_t tstart;    /**< Start time.            */
	uint64_t tshutdown; /**< Shutdown time.         */
	uint64_t talloc;    /**< Allocation time.       */
	uint64_t tfree;     /**< Free time.             */
	uint64_t tread;     /**< Read time.             */
	uint64_t twrite;    /**< Write time.            */
	unsigned nblocks;   /**< Blocks allocated       */
} stats = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/**
 * @brief Node number.
 */
static int nodenum;

/**
 * @brief Input mailbox for small messages.
 */
static int inbox;

/**
 * @brief Input portal for receiving data.
 */
static int inportal;

/**
 * @brief Remote memory.
 *
 * @todo TODO: allocate this dynamically with kernel calls.
 */
static char rmem[RMEM_NUM_BLOCKS][RMEM_BLOCK_SIZE];

/**
 * @brief Map of blocks.
 */
static bitmap_t blocks[RMEM_NUM_BLOCKS/BITMAP_WORD_LENGTH];

/*============================================================================*
 * do_rmem_alloc()                                                            *
 *============================================================================*/

/**
 * @brief Handles remote memory allocation.
 *
 * @returns Upon successful completion, the number of the newly
 * allocated remote memory block is allocated. Upon failure, @p
 * RMEM_NULL is returned instead.
 */
static inline rpage_t do_rmem_alloc(void)
{
	bitmap_t bit;

	/* Memory server is full. */
	if (stats.nblocks == RMEM_NUM_BLOCKS)
	{
	nanvix_printf("[nanvix][rmem] remote memory full\n");
		return (RMEM_NULL);
	}

	/* Find a free block. */
	nanvix_assert(
		(bit = bitmap_first_free(
			blocks,
			(RMEM_NUM_BLOCKS/BITMAP_WORD_LENGTH)*sizeof(bitmap_t)
		)) != BITMAP_FULL
	);

	/* Allocate block. */
	stats.nblocks++;
	bitmap_set(blocks, bit);
	rmem_debug("rmem_alloc() blknum=%d nblocks=%d/%d",
		bit, stats.nblocks, RMEM_NUM_BLOCKS
	);

	return (bit);
}

/*============================================================================*
 * do_rmem_free()                                                             *
 *============================================================================*/

/**
 * @brief Handles remote memory free.
 *
 * @param blknum Number of the target block.
 *
 * @returns Upon successful completion, zero is returned. Upon
 * failure, a negative error code is returned instead.
 */
static inline int do_rmem_free(rpage_t blknum)
{
	/* Invalid block number. */
	if ((blknum == RMEM_NULL) || (blknum >= RMEM_NUM_BLOCKS))
	{
		nanvix_printf("[nanvix][rmem] invalid block number\n");
		return (-EINVAL);
	}

	/* Remote memory is empty. */
	if (stats.nblocks == 1)
	{
		nanvix_printf("[nanvix][rmem] remote memory is empty\n");
		return (-EFAULT);
	}

	/* Bad block number. */
	if (!bitmap_check_bit(blocks, blknum))
	{
		nanvix_printf("[nanvix][rmem] bad free block\n");
		return (-EFAULT);
	}

	/* Clean block. */
	nanvix_memset(&rmem[blknum][0], 0, RMEM_BLOCK_SIZE);

	/* Free block. */
	stats.nblocks--;
	bitmap_clear(blocks, blknum);
	rmem_debug("rmem_free blknum=%d nblocks=%d/%d",
		blknum, stats.nblocks, RMEM_NUM_BLOCKS
	);

	return (0);
}

/*============================================================================*
 * do_rmem_write()                                                            *
 *============================================================================*/

/**
 * @brief Handles a write request.
 *
 * @param remote Remote client.
 * @param blknum Number of the target block.
 */
static inline int do_rmem_write(int remote, rpage_t blknum)
{
	int ret = 0;

	rmem_debug("write nodenum=%d blknum=%d",
		remote,
		blknum
	);

	/* Invalid block number. */
	if ((blknum == RMEM_NULL) || (blknum >= RMEM_NUM_BLOCKS))
	{
		nanvix_printf("[nanvix][rmem] invalid block number\n");
		return (-EINVAL);
	}

	/*
	 * Bad block number. Drop this read and return
	 * an error. Note that we use the NULL block for this.
	 */
	if (!bitmap_check_bit(blocks, blknum))
	{
		nanvix_printf("[nanvix][rmem] bad write block\n");
		blknum = 0;
		ret = -EFAULT;
	}

	nanvix_assert(kportal_allow(inportal, remote) == 0);
	nanvix_assert(
		kportal_read(
			inportal,
			&rmem[blknum][0],
			RMEM_BLOCK_SIZE
		) == RMEM_BLOCK_SIZE
	);

	return (ret);
}

/*============================================================================*
 * do_rmem_read()                                                             *
 *============================================================================*/

/**
 * @brief Handles a read request.
 *
 * @param remote Remote client.
 * @param blknum Number of the target block.
 *
 * @returns Upon successful completion, zero is returned. Upon
 * failure, a negative error code is returned instead.
 */
static inline int do_rmem_read(int remote, rpage_t blknum)
{
	int ret = 0;
	int outportal;

	rmem_debug("read nodenum=%d blknum=%d",
		remote,
		blknum
	);

	/* Invalid block number. */
	if ((blknum == RMEM_NULL) || (blknum >= RMEM_NUM_BLOCKS))
	{
		nanvix_printf("[nanvix][rmem] invalid block number\n");
		return (-EINVAL);
	}

	/*
	 * Bad block number. Let us send a null block
	 * and return an error instead.
	 */
	if (!bitmap_check_bit(blocks, blknum))
	{
		nanvix_printf("[nanvix][rmem] bad read block\n");
		blknum = 0;
		ret = -EFAULT;
	}

	nanvix_assert((outportal =
		kportal_open(
			knode_get_num(),
			remote)
		) >= 0
	);
	nanvix_assert(
		kportal_write(
			outportal,
			&rmem[blknum][0],
			RMEM_BLOCK_SIZE
		) == RMEM_BLOCK_SIZE
	);
	nanvix_assert(kportal_close(outportal) == 0);

	return (ret);
}

/*============================================================================*
 * do_rmem_loop()                                                             *
 *============================================================================*/

/**
 * @brief Handles remote memory requests.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_rmem_loop(void)
{
	int shutdown = 0;
	uint64_t t0, t1;

	kclock(&stats.tstart);

	while(!shutdown)
	{
		int source;
		struct rmem_message msg;

		nanvix_assert(
			kmailbox_read(
				inbox,
				&msg,
				sizeof(struct rmem_message)
			) == sizeof(struct rmem_message)
		);

		rmem_debug("rmem request source=%d opcode=%d",
			msg.header.source,
			msg.header.opcode
		);

		/* handle write operation. */
		switch (msg.header.opcode)
		{
			/* Write to RMEM. */
			case RMEM_WRITE:
				stats.nwrites++;
				kclock(&t0);
					msg.errcode = do_rmem_write(msg.header.source, msg.blknum);
					nanvix_assert((source = kmailbox_open(msg.header.source)) >= 0);
					nanvix_assert(kmailbox_write(source, &msg, sizeof(struct rmem_message)) == sizeof(struct rmem_message));
					nanvix_assert(kmailbox_close(source) == 0);
				kclock(&t1);
				stats.twrite += (t1 - t0);
				break;

			/* Read a page. */
			case RMEM_READ:
				stats.nreads++;
				kclock(&t0);
					msg.errcode = do_rmem_read(msg.header.source, msg.blknum);
					nanvix_assert((source = kmailbox_open(msg.header.source)) >= 0);
					nanvix_assert(kmailbox_write(source, &msg, sizeof(struct rmem_message)) == sizeof(struct rmem_message));
					nanvix_assert(kmailbox_close(source) == 0);
				kclock(&t1);
				stats.tread += (t1 - t0);
				break;

			/* Allocates a page. */
			case RMEM_ALLOC:
				stats.nallocs++;
				kclock(&t0);
					msg.blknum = do_rmem_alloc();
					msg.errcode = (msg.blknum == RMEM_NULL) ? -ENOMEM : 0;
					nanvix_assert((source = kmailbox_open(msg.header.source)) >= 0);
					nanvix_assert(kmailbox_write(source, &msg, sizeof(struct rmem_message)) == sizeof(struct rmem_message));
					nanvix_assert(kmailbox_close(source) == 0);
				kclock(&t1);
				stats.talloc += (t1 - t0);
			    break;

			/* Free frees a page. */
			case RMEM_MEMFREE:
				stats.nfrees++;
				kclock(&t0);
					msg.errcode = do_rmem_free(msg.blknum);
					nanvix_assert((source = kmailbox_open(msg.header.source)) >= 0);
					nanvix_assert(kmailbox_write(source, &msg, sizeof(struct rmem_message)) == sizeof(struct rmem_message));
					nanvix_assert(kmailbox_close(source) == 0);
				kclock(&t1);
				stats.tfree += (t1 - t0);
			    break;

			case RMEM_EXIT:
				kclock(&stats.tshutdown);
				shutdown = 1;
				break;

			/* Should not happen. */
			default:
				break;
		}
	}

	/* Dump statistics. */
	nanvix_printf("[nanvix][rmem] talloc=%d nallocs=%d tfree=%d nfrees=%d tread=%d nreads=%d twrite=%d nwrites=%d\n",
			stats.talloc, stats.nallocs,
			stats.tfree, stats.nfrees,
			stats.tread, stats.nreads,
			stats.twrite, stats.nallocs
	);

	return (0);
}

/*============================================================================*
 * do_rmem_startup()                                                          *
 *============================================================================*/

/**
 * @brief Initializes the remote memory server.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_rmem_startup(void)
{
	int ret;
	char pathname[NANVIX_PROC_NAME_MAX];

	/* Messages should be small enough. */
	nanvix_assert(sizeof(struct rmem_message) <= MAILBOX_MSG_SIZE);

	/* Bitmap word should be large enough. */
	nanvix_assert(sizeof(rpage_t) >= sizeof(bitmap_t));

	/* Clean bitmap. */
	nanvix_memset(
		blocks,
		0,
		(RMEM_NUM_BLOCKS/BITMAP_WORD_LENGTH)*sizeof(bitmap_t)
	);

	/* Fist block is special. */
	stats.nblocks++;
	bitmap_set(blocks, 0);

	/* Clean all blocks. */
	for (unsigned long i = 0; i < RMEM_NUM_BLOCKS; i++)
		nanvix_memset(&rmem[i][0], 0, RMEM_BLOCK_SIZE);

	nodenum = knode_get_num();

	/* Assign input mailbox. */
	inbox = stdinbox_get();

	/* Assign input portal. */
	inportal = stdinportal_get();

	/* Link name. */
	nanvix_strcpy(pathname, "/rmem");
	if ((ret = name_link(nodenum, pathname)) < 0)
		return (ret);

	return (0);
}

/*============================================================================*
 * do_rmem_shutdown()                                                         *
 *============================================================================*/

/**
 * @brief Shutdowns the remote memory server.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
static int do_rmem_shutdown(void)
{
	return (0);
}

/*============================================================================*
 * do_rmem_server()                                                           *
 *============================================================================*/

/**
 * @brief Remote memory server.
 *
 * @returns Upon successful completion zero is returned. Upon failure,
 * a negative error code is returned instead.
 */
int do_rmem_server(void)
{
	int ret;

	nanvix_printf("[nanvix][rmem] booting up server\n");

	if ((ret = do_rmem_startup()) < 0)
		goto error;

	/* Unblock spawner. */
	nanvix_assert(stdsync_fence() == 0);
	nanvix_printf("[nanvix][rmem] server alive\n");

	if ((ret = do_rmem_loop()) < 0)
		goto error;

	nanvix_printf("[nanvix][rmem] shutting down server\n");

	if ((ret = do_rmem_shutdown()) < 0)
		goto error;

	return (0);

error:
	return (ret);
}

/*============================================================================*
 * __main2()                                                                  *
 *============================================================================*/

/**
 * @brief Handles remote memory requests.
 *
 * @param argc Argument count (unused).
 * @param argv Argument list (unused).
 *
 * @returns Always returns zero.
 */
int __main2(int argc, const char *argv[])
{
	((void) argc);
	((void) argv);

	__runtime_setup(1);

	do_rmem_server();

	__runtime_cleanup();

	return (0);
}
