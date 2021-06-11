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

#include <nanvix/runtime/runtime.h>
#include <nanvix/sys/perf.h>
#include <nanvix/ulib.h>
#include <nanvix/pm.h>
#include <nanvix/config.h>
#include <nanvix/fs.h>
#include <nanvix/limits.h>
#include <nanvix/ulib.h>
#include <posix/errno.h>
#include <posix/fcntl.h>
#include <posix/unistd.h>
#include <posix/stdlib.h>
#include "../test.h"

#ifdef __NANVIX_HAS_VFS_SERVER

/**
 * @brief File Offset for Tests
 */
#define TEST_FILE_OFFSET (8*NANVIX_FS_BLOCK_SIZE)

/**
 * @brief Number of iterations
 */
#define N_ITERATIONS 600

/**
 * @brief Number of warm up iterations
 */
#define WARMUP 5

/**
 * @brief Maximum byte size for read/write tests
 */
#define RW_BYTE_LIMIT 65536

/**
 * @brief Optimal size for read/write tests
 */
#define OPTIMAL_BYTE_N 4096

/**
 * @brief Maximum number of processes in read/write tests
 */
#define MAX_PROCESSES 8

/**
 * @brief Buffer for Read/Write Tests
 */
static char *data;

/*============================================================================*
 * Open/Close                                                                 *
 *============================================================================*/

/**
 * @brief Benchmark Test: Open/Close a File
 */
static void benchmark_nanvix_vfs_open_close(void)
{
	int fd;
	uint64_t t1;
	uint64_t t2;
	const char *filename = "disk";

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert((fd = nanvix_vfs_open(filename, O_RDONLY, 0)) >= 0);
		uassert(nanvix_vfs_close(fd) == 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][open/close read only] %l", (t2 - t1));
	}

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert((fd = nanvix_vfs_open(filename, O_WRONLY, 0)) >= 0);
		uassert(nanvix_vfs_close(fd) == 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][open/close write only] %l", (t2 - t1));
	}

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert((fd = nanvix_vfs_open(filename, O_RDWR, 0)) >= 0);
		uassert(nanvix_vfs_close(fd) == 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][open/close read write] %l", (t2 - t1));
	}
}

/*============================================================================*
 * Stat                                                                       *
 *============================================================================*/

/**
 * @brief Benchmark Test: Get File Stats
 */
static void benchmark_nanvix_stat(void)
{
	uint64_t t1;
	uint64_t t2;
	struct nanvix_stat buffer;
	const char *filename = "disk";

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();

		uassert((nanvix_vfs_stat(filename, &buffer)) >= 0);

		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][stat file] %l", (t2 - t1));
	}
}

/*============================================================================*
 * Seek                                                                       *
 *============================================================================*/

/**
 * @brief Benchmark Test: Seek Read/Write Pointer of a File
 */
static void benchmark_nanvix_vfs_seek(void)
{
	int fd;
	uint64_t t1;
	uint64_t t2;
	const char *filename = "disk";

	uassert((fd = nanvix_vfs_open(filename, O_RDWR, 0)) >= 0);

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert(nanvix_vfs_seek(fd, NANVIX_FS_BLOCK_SIZE, SEEK_CUR) >= 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][seek cur] %l", (t2 - t1));
	}

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert(nanvix_vfs_seek(fd, 0, SEEK_END) >= 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][seek end] %l", (t2 - t1));
	}

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();
		uassert(nanvix_vfs_seek(fd, NANVIX_FS_BLOCK_SIZE , SEEK_SET) >= 0);
		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][seek set] %l", (t2 - t1));
	}

	uassert(nanvix_vfs_close(fd) == 0);
}

/*============================================================================*
 * Read/Write                                                                 *
 *============================================================================*/

/**
 * @brief Benchmark Test: Read/Write from/to a File variating buffer size
 */
static void benchmark_nanvix_vfs_read_write(void)
{
	int fd;
	uint64_t t1;
	uint64_t t2;
	const char *filename = "disk";
	const char *regfilename = "rdwr_file";

	data = nanvix_malloc(RW_BYTE_LIMIT);
	uassert((fd = nanvix_vfs_open(filename, O_RDWR, 0)) >= 0);

	/* i is # of bytes to read/write and doubles each iteration */
	for (int i=256; i<=RW_BYTE_LIMIT; i*=2) {
		/* Write */
		umemset(data, 1, RW_BYTE_LIMIT);
		uassert(nanvix_vfs_seek(fd, TEST_FILE_OFFSET, SEEK_SET) >= 0);
		for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
			t1 = clock_read();
			uassert(nanvix_vfs_write(fd, data, i) == i);
			t2 = clock_read();
			if (j > WARMUP)
				uprintf("[benchmarks][blk file write (%d bytes)] %l", i, (t2 - t1));
		}

		/* Read. */
		umemset(data, 0, RW_BYTE_LIMIT);
		uassert(nanvix_vfs_seek(fd, TEST_FILE_OFFSET, SEEK_SET) >= 0);
		for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
			t1 = clock_read();
			uassert(nanvix_vfs_read(fd, data, i) == i);
			t2 = clock_read();
			if (j > WARMUP)
				uprintf("[benchmarks][blk file read (%d bytes)] %l", i,(t2 - t1));
		}

		/* Checksum. */
		for (size_t j = 0; j < (size_t)i && j < sizeof(data); j++)
			uassert(data[j] == 1);
	}

	uassert(nanvix_vfs_close(fd) == 0);

	/* Regular file */
	uassert((fd = nanvix_vfs_open(regfilename, (O_RDWR | O_CREAT),
					(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) >= 0);
	for (int i=256; i<=RW_BYTE_LIMIT; i*=2) {
		/* Write */
		umemset(data, 1, RW_BYTE_LIMIT);
		uassert(nanvix_vfs_seek(fd, TEST_FILE_OFFSET, SEEK_SET) >= 0);
		for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
			t1 = clock_read();
			uassert(nanvix_vfs_write(fd, data, i) == i);
			t2 = clock_read();
			if (j > WARMUP)
				uprintf("[benchmarks][reg file write (%d bytes)] %l", i, (t2 - t1));
		}

		/* Read. */
		umemset(data, 0, RW_BYTE_LIMIT);
		uassert(nanvix_vfs_seek(fd, TEST_FILE_OFFSET, SEEK_SET) >= 0);
		for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
			t1 = clock_read();
			uassert(nanvix_vfs_read(fd, data, i) == i);
			t2 = clock_read();
			if (j > WARMUP)
				uprintf("[benchmarks][reg file read (%d bytes)] %l", i, (t2 - t1));
		}

		/* Checksum. */
		for (size_t j = 0; j < (size_t)i && j < sizeof(data); j++)
			uassert(data[j] == 1);

	}
	uassert(nanvix_vfs_close(fd) == 0);
	uassert(nanvix_vfs_unlink(regfilename) == 0);
	nanvix_free(data);
}

/**
 * @brief Benchmark Test: Read/Write from/to a File with fixed buffer size in OPTIMAL_BYTE_N
 */
static void benchmark_nanvix_vfs_rw_optimal(void)
{
	int fd;
	uint64_t t1;
	uint64_t t2;
	char *regfilename = nanvix_malloc(sizeof(char) * 10);

	/* random file number suffix so each process reads/writes to/from a different file */
	usprintf(regfilename, "rw_file%d", knode_get_num());

	data = nanvix_malloc(OPTIMAL_BYTE_N);

	/* Regular file */
	uassert((fd = nanvix_vfs_open(regfilename, (O_RDWR | O_CREAT),
					(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) >= 0);
	uprintf("RANDOM NAME: %s %d", regfilename, fd);

	/* Write */
	umemset(data, 1, RW_BYTE_LIMIT);
	for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
		t1 = clock_read();
		uassert(nanvix_vfs_write(fd, data, OPTIMAL_BYTE_N) == OPTIMAL_BYTE_N);
		t2 = clock_read();
		if (j > WARMUP)
			uprintf("[benchmarks][reg file write (%d bytes)] %l", OPTIMAL_BYTE_N, (t2 - t1));
	}

	/* Read. */
	umemset(data, 0, RW_BYTE_LIMIT);
	for (int j=0; j < N_ITERATIONS + WARMUP; ++j) {
		t1 = clock_read();
		uassert(nanvix_vfs_read(fd, data, OPTIMAL_BYTE_N) == OPTIMAL_BYTE_N);
		t2 = clock_read();
		if (j > WARMUP)
			uprintf("[benchmarks][reg file read (%d bytes)] %l", OPTIMAL_BYTE_N, (t2 - t1));
	}

	/* Checksum. */
	for (size_t j = 0; j < sizeof(data); j++)
		uassert(data[j] == 1);

	uassert(nanvix_vfs_close(fd) == 0);
	uassert(nanvix_vfs_unlink(regfilename) == 0);
	nanvix_free(data);
}

/**
 * @brief Benchmark Test: Create a file
 */
static void benchmark_nanvix_vfs_creat_unlink(void)
{
	int fd;
	uint64_t t1;
	uint64_t t2;
	const char *filename = "new_file";

	for (int i=0; i < N_ITERATIONS + WARMUP; ++i) {
		t1 = clock_read();

		uassert((fd = nanvix_vfs_open(filename, (O_CREAT | O_WRONLY), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) >= 0);

		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][create file] %l", (t2 - t1));

		uassert(nanvix_vfs_close(fd) == 0);

		t1 = clock_read();

		uassert(nanvix_vfs_unlink(filename) == 0);

		t2 = clock_read();
		if (i > WARMUP)
			uprintf("[benchmarks][unlink file] %l", (t2 - t1));
	}

}

/*============================================================================*
 * Benchmark Tests                                                                  *
 *============================================================================*/

/**
 * @brief Virtual File System Tests
 */
struct test tests_vfs_benchmark[] = {
	{ benchmark_nanvix_vfs_open_close,   "[vfs][benchmark] open/close        " },
	{ benchmark_nanvix_vfs_seek,         "[vfs][benchmark] seek              " },
	{ benchmark_nanvix_vfs_read_write,   "[vfs][benchmark] read/write        " },
	{ benchmark_nanvix_vfs_rw_optimal,   "[vfs][benchmark] read/write optimal" },
	{ benchmark_nanvix_stat,             "[vfs][benchmark] stat              " },
	{ benchmark_nanvix_vfs_creat_unlink, "[vfs][benchmark] creat/unlink      " },
	{ NULL,                            NULL                                    },
};

#endif
