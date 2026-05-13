#include "PageManager.h"
#include "Allocator.h"

unsigned int PageManager::PHY_MEM_SIZE;
unsigned int UserPageManager::USER_PAGE_POOL_SIZE;

PageManager::PageManager(BitMapAllocator* allocator)
{
	this->m_pAllocator = allocator;
}

int PageManager::Initialize()
{
	for ( unsigned int i = 0; i < MEMORY_MAP_ARRAY_SIZE; i++ ) 
	{
		this->m_pAllocator->set_bit(this->bitmap, i, 0);
	}
	for (int i = 0; i < MemoryDescriptor::USER_SPACE_SIZE / PAGE_SIZE; i++)
	{
		Page[i] = 0;
	}
	return 0;
}

unsigned long PageManager::AllocMemory(unsigned long size)
{
	unsigned long newaddr = this->m_pAllocator->Alloc(this->bitmap, size);
	Page[newaddr >> 12] = 1;
	return newaddr;
}

unsigned long PageManager::FreeMemory(unsigned long size, unsigned long startAddress)
{
	int frame = startAddress >> 12;
	this->Page[frame]--;
	if (this->Page[frame] == 0)
	{
    	this->m_pAllocator->Free(this->bitmap, size, startAddress);
	}
	return 1;
}

PageManager::~PageManager()
{
}

KernelPageManager::KernelPageManager(BitMapAllocator *allocator)
	: PageManager(allocator)
{
}

int KernelPageManager::Initialize()
{
	PageManager::Initialize();

	this->bitmap.set(KERNEL_PAGE_POOL_START_ADDR, KERNEL_PAGE_POOL_SIZE / PageManager::PAGE_SIZE);

	return 0;
}

UserPageManager::UserPageManager(BitMapAllocator *allocator)
	: PageManager(allocator)
{
}

int UserPageManager::Initialize()
{
	PageManager::Initialize();

	this->bitmap.set(USER_PAGE_POOL_START_ADDR, USER_PAGE_POOL_SIZE / PageManager::PAGE_SIZE);

	return 0;
}

