#include "Text.h"
#include "Kernel.h"

Text::Text()
{
	/*this->x_daddr = 0;
	this->x_size = 0;
	this->x_iptr = NULL;
	this->x_count = 0;
	this->x_ccount = 0;
	for (int i = 0; i < TEXT_PAGE_MAX; i++)
	{
		this->x_caddr[i] = 0;
	}*/
}

Text::~Text()
{
	// nothing to do here
}

void Text::XccDec()
{
	User &u = Kernel::Instance().GetUser();
	PageTable *pUserPageTable = u.u_MemoryDescriptor.m_UserPageTableArray;
	UserPageManager &userPageMgr = Kernel::Instance().GetUserPageManager();

	if (this->x_ccount == 0)
		return;

	--this->x_ccount;

	int index = (u.u_MemoryDescriptor.m_TextStartAddress >> 12) - PageTable::ENTRY_CNT_PER_PAGETABLE;
	int count = (u.u_MemoryDescriptor.m_TextSize + PageManager::PAGE_SIZE - 1) / PageManager::PAGE_SIZE;
	int frame;
	// for (int i = 0; i < count && i < TEXT_PAGE_MAX; i++, index++)
	while (count)
	{
		if (this->x_ccount == 0)
		{
			frame = pUserPageTable->m_Entrys[index].m_PageBaseAddress;
			userPageMgr.FreeMemory(PageManager::PAGE_SIZE, frame << 12);
			// this->x_caddr[i] = 0;
		}

		pUserPageTable->m_Entrys[index].m_Present = 0;
		pUserPageTable->m_Entrys[index].m_ReadWriter = 0;
		pUserPageTable->m_Entrys[index].m_UserSupervisor = 1;
		pUserPageTable->m_Entrys[index].m_PageBaseAddress = 0;

		index++;
		count--;
	}
}

void Text::XFree()
{
	this->XccDec();
	/*
	 * 如果引用该共享正文段的进程数为0，进程都已终止
	 * 则没有必要在交换区上保存共享正文段的副本。
	 */
	if (--this->x_count == 0)
	{
		Kernel::Instance().GetSwapperManager().FreeSwap(this->x_size, this->x_daddr);
		Kernel::Instance().GetFileManager().m_InodeTable->IPut(this->x_iptr);
		this->x_iptr = NULL;
	}
}
