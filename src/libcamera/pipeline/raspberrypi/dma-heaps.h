/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2019, Raspberry Pi (Trading) Limited
 *
 * dma-heap.h - Helper class for dma-heap allocations.
 */
#pragma once

#include <iostream>
#include <mutex>

#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace RPi {

// /dev/dma-heap/linux,cma is the dma-heap allocator, which allows dmaheap-cma to
// only have to worry about importing.
// Annoyingly, should the cma heap size be specified on the kernel command line
// instead of DT, the heap gets named "reserved" instead.
#define DMA_HEAP_CMA_NAME  "/dev/dma_heap/linux,cma"
#define DMA_HEAP_CMA_ALT_NAME  "/dev/dma_heap/reserved"

class Dmaheap
{
public:
	Dmaheap()
	{
		dmaheapHandle_ = ::open(DMA_HEAP_CMA_NAME, O_RDWR, 0);
		if (dmaheapHandle_ == -1) {
			dmaheapHandle_ = ::open(DMA_HEAP_CMA_ALT_NAME, O_RDWR, 0);
			if (dmaheapHandle_ == -1) {
				std::cerr << "Could not open dmaheap device";
			}
		}
	}

	~Dmaheap()
	{
		/* Free all existing allocations. */
		auto it = allocMap_.begin();
		while (it != allocMap_.end())
			it = remove(it->first);

		if (dmaheapHandle_)
			::close(dmaheapHandle_);
	}

	void *alloc(const char *name, unsigned int size)
	{
		unsigned int pageSize = getpagesize();
		void *user_ptr;
		int ret;

		/* Ask for page aligned allocation. */
		size = (size + pageSize - 1) & ~(pageSize - 1);

		struct dma_heap_allocation_data alloc;

		memset(&alloc, 0, sizeof(alloc));
		alloc.len = size,
		alloc.fd_flags = O_CLOEXEC,

		ret = ::ioctl(dmaheapHandle_, DMA_HEAP_IOCTL_ALLOC, &alloc);

		if (ret < 0) {
			std::cerr << "dmaheap allocation failure for "
				  << name << std::endl;
			return nullptr;
		}
		ret = ::ioctl(alloc.fd, DMA_BUF_SET_NAME, name);

		/* Map the buffer into user space. */
		user_ptr = ::mmap(0, size, PROT_READ | PROT_WRITE,
				  MAP_SHARED, alloc.fd, 0);

		if (user_ptr == MAP_FAILED) {
			std::cerr << "dmaheap mmap failure for " << name << std::endl;
			::close(alloc.fd);
			return nullptr;
		}

		std::lock_guard<std::mutex> lock(lock_);
		allocMap_.emplace(user_ptr, AllocInfo(alloc.fd,
						      size));

		return user_ptr;
	}

	void free(void *user_ptr)
	{
		std::lock_guard<std::mutex> lock(lock_);
		remove(user_ptr);
	}

	unsigned int getHandle(void *user_ptr)
	{
		std::lock_guard<std::mutex> lock(lock_);
		auto it = allocMap_.find(user_ptr);
		if (it != allocMap_.end())
			return it->second.handle;

		return 0;
	}

private:
	struct AllocInfo {
		AllocInfo(int handle_, int size_)
			: handle(handle_), size(size_)
		{
		}

		int handle;
		int size;
	};

	/* Map of all allocations that have been requested. */
	using AllocMap = std::map<void *, AllocInfo>;

	AllocMap::iterator remove(void *user_ptr)
	{
		auto it = allocMap_.find(user_ptr);
		if (it != allocMap_.end()) {
			int handle = it->second.handle;
			int size = it->second.size;
			::munmap(user_ptr, size);
			::close(handle);
			/*
			 * Remove the allocation from the map. This returns
			 * an iterator to the next element.
			 */
			it = allocMap_.erase(it);
		}

		/* Returns an iterator to the next element. */
		return it;
	}

	AllocMap allocMap_;
	int dmaheapHandle_;
	std::mutex lock_;
};

} /* namespace RPi */
