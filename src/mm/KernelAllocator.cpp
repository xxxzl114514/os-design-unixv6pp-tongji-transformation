#include "KernelAllocator.h"
#include "Allocator.h"

KernelAllocator::KernelAllocator(BitMapAllocator *allocator)
{
	this->m_pAllocator = allocator;
}

int KernelAllocator::Initialize()
{
	for (unsigned int i = 0; i < MEMORY_MAP_ARRAY_SIZE; i++)
	{
		this->m_pAllocator->set_bit(this->map, 0, 0);
	}
	this->map.set(KERNEL_HEAP_START_ADDR, KERNEL_HEAP_SIZE / M_PAGE_SIZE);

	return 0;
}

unsigned long KernelAllocator::AllocMemory(unsigned long size)
{
	return this->m_pAllocator->Alloc(this->map, size);
}

unsigned long KernelAllocator::FreeMemeory(unsigned long size, unsigned long memoryStartAddress)
{
	return this->m_pAllocator->Free(this->map, size, memoryStartAddress);
}

KernelAllocator::~KernelAllocator()
{
}

