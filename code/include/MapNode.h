#ifndef MAP_NODE_H
#define MAP_NODE_H

/*
 *@comment 这个结构对应Unixv6中的map结构
 *下面给出map结构参考
 * struct map	@line 2515
 * {
	char *m_size;
	char *m_addr;
 * }
 */
struct MapNode
{
	unsigned long m_Size;
	/*
	 * 注释可能是不对的。
	 * m_addr 表示数据块在整个空间中的索引位置，
	 * 例如physical内存中4k一块，若m_AddressIdx为2，
	 * 则表示0x2000(8k)的位置，同理swap区中，
	 * 数据块大小为512byte
	 */
	unsigned long m_AddressIdx; // 分配空间的起始地址
};

#define M_PAGE_SIZE 4096

#define M_PAGE_CNT 0x100000

class BitMap
{
public:
	unsigned long m_AddressIdx;					 // 分配空间的起始地址, 不变
	int rows;									 // 64位一行，总共多少行
	int pages_left;								 // 最后一行，还可以放多少页
	unsigned long long int map[M_PAGE_CNT / 64]; // TODO：实现大小根据需要开辟

	void set(unsigned long startAddr, unsigned long page_num)
	{
		m_AddressIdx = startAddr;
		rows = page_num / 64;
		pages_left = page_num % 64;
		for (int i = 0; i < rows + 1; i++)
		{
			map[i] = 0;
		}
	};
};

class SwapperBitMap
{
public:
	unsigned long m_AddressIdx;			   // 分配空间的起始地址, 不变
	int rows;							   // 64位一行，总共多少行
	int pages_left;						   // 最后一行，还可以放多少页
	unsigned long long int map[2048 / 64]; // TODO：实现大小根据需要开辟

	void set(unsigned long startAddr, unsigned long page_num)
	{
		m_AddressIdx = startAddr;
		rows = page_num / 64;
		pages_left = page_num % 64;
		for (int i = 0; i < rows + 1; i++)
		{
			map[i] = 0;
		}
	};
};

#endif