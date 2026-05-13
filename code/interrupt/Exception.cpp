#include "Exception.h"
#include "Kernel.h"
#include "Utility.h"
#include "Video.h"
#include "Machine.h"

/*
 * 声明INT 0 - INT 31号异常在IDT中的入口函数(Entrance)
 * -->无出错码<-- 的异常
 */
#define IMPLEMENT_EXCEPTION_ENTRANCE(Exception_Entrance, Exception_Handler) \
	void Exception::Exception_Entrance()                                    \
	{                                                                       \
		SaveContext();                                                      \
                                                                            \
		SwitchToKernel();                                                   \
                                                                            \
		CallHandler(Exception, Exception_Handler);                          \
                                                                            \
		RestoreContext();                                                   \
                                                                            \
		Leave();                                                            \
                                                                            \
		InterruptReturn();                                                  \
	}

/*
 * 声明INT 0 - INT 31号异常在IDT中的入口函数(Entrance)
 * -->有出错码(ErrCode)<-- 的异常
 * 由于出错码必须在iret中断返回指令之前手动从栈上弹出，
 * 所以有出错码的情况下，在leave指令销毁栈帧后，再跳过
 * 栈上的4个字节出错码。
 */
#define IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(Exception_Entrance, Exception_Handler) \
	void Exception::Exception_Entrance()                                            \
	{                                                                               \
		SaveContext();                                                              \
                                                                                    \
		SwitchToKernel();                                                           \
                                                                                    \
		CallHandler(Exception, Exception_Handler);                                  \
                                                                                    \
		RestoreContext();                                                           \
                                                                                    \
		Leave();                                                                    \
                                                                                    \
		__asm__ __volatile__("addl $4, %%esp" ::);                                  \
                                                                                    \
		InterruptReturn();                                                          \
	}

/*
	=========================
	对上面代码的另一点说明：
	=========================
	目前，由于我们在IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE和IMPLEMENT_EXCEPTION_ENTRANCE中采用的都是
	Inline Assembly，没有声明任何临时、局部变量(如int i, j;之类的语句)或者函数调用，所以入口函数(Entrance)中除了在栈上压
	入ebp之外，没有在栈上分配任何多余的字节，否则就可能出现下面的情况：

	EFLAGS
	CS
	EIP
	[ERRORCODE]	//Optional
	ebp
	xx字节空间  	<--”临时、局部变量占用的堆栈空间“
	SaveContext();

	这会导致如果打算采用某一个结构体，如pt_regs来包括通用寄存器(eax,ebx...等)和ERRORCODE，eip，cs，eflags的全部字段，
	那么在SaveContext()部分字段和ERRORCODE，eip，cs，eflags之间间隙应该预留多少字节的填充字段(padding)？结构体中应该
	预留多少字节长度的填充字段是无法预先计算得到的，而且长度会随着函数中声明局部变量多少而由编译器自动确定。
	我们并没有只采用一个结构体pt_regs来包括全部的字段，而是采用了pt_regs和pt_context两个字段；pt_regs包含了通用寄存器
	中的现场信息，而pt_context则包括了中断隐指令保存的现场(eflags，cs，eip和[ERRORCODE])。此外，在SaveContext()宏的实
	现中，采用

	=========================
	关于leave指令的一点说明：
	=========================
	不宜在X86Assembly类中实现对leave和iret指令封装的函数。
	对leave和iret这2条指令进行函数封装，在调用时会产生一些问题，所以上面的**Entrance()宏里面直接使用宏封装的内联汇编。
	X86Assembly::Leave()函数的反汇编结果将会如下：

	push   %ebp
	mov    %esp,%ebp
	leave
	pop    %ebp
	ret

	leave指令等价于2条指令: mov %ebp, %esp; pop ebp; 其作用是销毁当前函数调用的栈帧，这里X86Assembly::Leave()函数中的leave
	指令销毁的是我们调用X86Assembly::Leave()产生的栈帧。这并非我们的本意，我们在RestoreContext();之后使用leave指令的目的是，
	当位于栈顶的是ebp时(即异常入口函数(Entrance)编译生成的第一条指令压入的ebp)，用leave指令使得ebp从栈中弹出以及恢复esp，从而销毁
	异常入口函数(Entrance)的栈帧。
	因而，目前唯一的解决办法是直接用Inline Assembly，而不对leave指令进行函数封装。

	=========================
	关于iret指令的一点说明：
	=========================
	我们需要在当栈顶存放的元素依次是:
	SS
	ESP
	EFLAGS
	CS
	EIP	<--  当前栈顶位置

	才可以使用iret指令从中断返回，而对iret指令进行封装X86Assembly::IRet()的结果则是，堆栈情况如下：
	SS
	ESP
	EFLAGS
	CS
	EIP
	ebp	<--  当前栈顶位置， ebp是X86Assembly::IRet()函数第一条指令压入的
	在以ebp为栈顶的情况下进行iret，会发生严重错误，错误地把ebp当成EIP, EIP当成CS，执行iret指令将导致系统崩溃。
*/

/*
 * 定义INT 0 - INT 31号异常处理函数(Handler)的2个宏。
 *
 * (1)	IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(Exception_Handler, Error_Message, Signal_Value)
 * 对应有出错码的情况，第二个参数使用struct pte_context* context;
 *
 * (2)	IMPLEMENT_EXCEPTION_HANDLER(Exception_Handler, Error_Message, Signal_Value)
 * 对应无出错码的情况，第二个参数使用struct pt_context* context;
 *
 * 两个宏的区别就在于第二参数是包含error_code的结构体pte_context, 还是没有
 * error_code字段的结构体pt_context!
 */

#define IMPLEMENT_EXCEPTION_HANDLER(Exception_Handler, Error_Message, Signal_Value)     \
	void Exception::Exception_Handler(struct pt_regs *regs, struct pt_context *context) \
	{                                                                                   \
		User &u = Kernel::Instance().GetUser();                                         \
		Process *current = u.u_procp;                                                   \
                                                                                        \
		if ((context->xcs & USER_MODE) == USER_MODE)                                    \
		{                                                                               \
			current->PSignal(Signal_Value);                                             \
			if (current->IsSig())                                                       \
				current->PSig(context);                                                 \
		}                                                                               \
		else                                                                            \
		{                                                                               \
			Utility::Panic(Error_Message);                                              \
		}                                                                               \
	}

#define IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(Exception_Handler, Error_Message, Signal_Value) \
	void Exception::Exception_Handler(struct pt_regs *regs, struct pte_context *context)    \
	{                                                                                       \
		User &u = Kernel::Instance().GetUser();                                             \
		Process *current = u.u_procp;                                                       \
                                                                                            \
		if ((context->xcs & USER_MODE) == USER_MODE)                                        \
		{                                                                                   \
			current->PSignal(Signal_Value);                                                 \
			if (current->IsSig())                                                           \
				current->PSig((pt_context *)&context->eip);                                 \
		}                                                                                   \
		else                                                                                \
		{                                                                                   \
			Utility::Panic(Error_Message);                                                  \
		}                                                                                   \
	}

Exception::Exception()
{
	// NOTHING IS OK
}

Exception::~Exception()
{
	// NOTHING IS OK
}

// 除零错(INT 0)
IMPLEMENT_EXCEPTION_ENTRANCE(DivideErrorEntrance, DivideError)
IMPLEMENT_EXCEPTION_HANDLER(DivideError, "Divide Exception!", User::SIGFPE)

// 调试异常(INT 1)
IMPLEMENT_EXCEPTION_ENTRANCE(DebugEntrance, Debug)
IMPLEMENT_EXCEPTION_HANDLER(Debug, "Debug Exception!", User::SIGTRAP)

// NMI非屏蔽中断(INT 2)
IMPLEMENT_EXCEPTION_ENTRANCE(NMIEntrance, NMI)
IMPLEMENT_EXCEPTION_HANDLER(NMI, "Non-maskable Interrupt!", User::SIGNUL)

// 调试断点(INT 3)
IMPLEMENT_EXCEPTION_ENTRANCE(BreakpointEntrance, Breakpoint)
IMPLEMENT_EXCEPTION_HANDLER(Breakpoint, "Breakpoint Exception!", User::SIGTRAP)

// 溢出(INT 4)
IMPLEMENT_EXCEPTION_ENTRANCE(OverflowEntrance, Overflow)
IMPLEMENT_EXCEPTION_HANDLER(Overflow, "Overflow Exception!", User::SIGSEGV)

// BOUND指令异常(INT 5)
IMPLEMENT_EXCEPTION_ENTRANCE(BoundEntrance, Bound)
IMPLEMENT_EXCEPTION_HANDLER(Bound, "Bound Range Exceeded!", User::SIGSEGV)

// 无效操作码(INT 6)
IMPLEMENT_EXCEPTION_ENTRANCE(InvalidOpcodeEntrance, InvalidOpcode)
IMPLEMENT_EXCEPTION_HANDLER(InvalidOpcode, "Invalid Opcode!", User::SIGILL)

// 设备不可用(INT 7)
IMPLEMENT_EXCEPTION_ENTRANCE(DeviceNotAvailableEntrance, DeviceNotAvailable)
IMPLEMENT_EXCEPTION_HANDLER(DeviceNotAvailable, "Device Not Available!", User::SIGSEGV)

// 双重错误(INT 8)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(DoubleFaultEntrance, DoubleFault)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(DoubleFault, "Double Fault Exception!", User::SIGSEGV)

// 协处理器段越界(INT 9)
IMPLEMENT_EXCEPTION_ENTRANCE(CoprocessorSegmentOverrunEntrance, CoprocessorSegmentOverrun)
IMPLEMENT_EXCEPTION_HANDLER(CoprocessorSegmentOverrun, "Coprocessor Segment Overrun!", User::SIGFPE)

// 无效TSS(INT 10)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(InvalidTSSEntrance, InvalidTSS)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(InvalidTSS, "Invalid TSS!", User::SIGSEGV)

// 段不存在(INT 11)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(SegmentNotPresentEntrance, SegmentNotPresent)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(SegmentNotPresent, "Segment Not Present!", User::SIGBUS)

// 堆栈段错误(INT 12)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(StackSegmentErrorEntrance, StackSegmentError)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(StackSegmentError, "Stack Segment Error!", User::SIGBUS)

// 一般保护性异常(INT 13)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(GeneralProtectionEntrance, GeneralProtection)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(GeneralProtection, "General Protection!", User::SIGSEGV)

// 缺页异常(INT 14)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(PageFaultEntrance, PageFault)
// IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(PageFault, "Page Fault!", User::SIGSEGV)

void Exception::PageFault(struct pt_regs *regs, struct pte_context *context)
{

	User &u = Kernel::Instance().GetUser();
	Process *current = u.u_procp;
	MemoryDescriptor &md = u.u_MemoryDescriptor;

	unsigned int cr2;
	__asm__ __volatile__(" mov %%cr2, %0" : "=r"(cr2));

	unsigned char RW = md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_ReadWriter;

	// 思路：需要处理 COW 的情况是缺页异常发生在数据段或者堆栈段 &R/W 位内容为 Read Only &引用计数器（Page）>= 1
	Process *pcurrent = NULL;
	UserPageManager &userPageMgr = Kernel::Instance().GetUserPageManager();
	ProcessManager &procMgr = Kernel::Instance().GetProcessManager();
	bool isWriteFault = (context->error_code & 0x2) != 0;
	bool isRW = (cr2 >= md.m_DataStartAddress && cr2 < md.m_DataStartAddress + md.m_DataSize) ||
				(cr2 >= MemoryDescriptor::USER_SPACE_SIZE - md.m_StackSize && cr2 < MemoryDescriptor::USER_SPACE_SIZE);
	unsigned int physicalBase = md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_PageBaseAddress;
	// 子进程和父进程没有区别，任意先上台的进程，申请新的空间，复制原本的内容使用
	// 如果是子进程，则是申请新空间，指向新空间
	// 如果是父进程，则可以理解为，将原本的空间让给子进程，自己利用新申请的空间
	if (RW == 0 && userPageMgr.Page[physicalBase] >= 1 && isRW)
	{
		if (userPageMgr.Page[physicalBase] == 1)
		{
			md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_ReadWriter = 1;
			return;
		}
		else
		{
			--userPageMgr.Page[physicalBase];
			unsigned long newPage = userPageMgr.AllocMemory(M_PAGE_SIZE);
			md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_PageBaseAddress = newPage >> 12;
			md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_UserSupervisor = 0x1;
			md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_Present = 0x1;
			md.m_UserPageTableArray->m_Entrys[(cr2 >> 12) - 1024].m_ReadWriter = 1;
			userPageMgr.Page[newPage >> 12] = 1;

			for (int i = 0; i < procMgr.NPROC; ++i)
			{
				if (procMgr.process[i].p_pid == current->p_ppid)
				{
					pcurrent = &procMgr.process[i];
					break;
				}
			}

			Utility::CopySeg(physicalBase << 12, newPage);
			return;
		}
	}

	/*由缺页异常处理程序每次扩展一页，如果合理的缺了多张堆栈页面，那就多执行几次缺页异常，直到把这些页面补齐*/

	if ((context->xcs & USER_MODE) == USER_MODE)
	{
		if (cr2 < MemoryDescriptor::USER_SPACE_SIZE - md.m_StackSize && cr2 >= context->esp - 8 && md.m_DataSize + md.m_StackSize + PageManager::PAGE_SIZE < MemoryDescriptor::USER_SPACE_SIZE - md.m_DataStartAddress)
			current->SStack();
		else
		{
			Diagnose::Write("Invalid MM access");
			current->PSignal(User::SIGSEGV);
			if (current->IsSig())
				current->PSig((pt_context *)&context->eip);
		}
	}
	else
		Utility::Panic("Page Fault in Kernel Mode.");
}

// x87 FPU浮点错误(INT 16)
IMPLEMENT_EXCEPTION_ENTRANCE(CoprocessorErrorEntrance, CoprocessorError)
IMPLEMENT_EXCEPTION_HANDLER(CoprocessorError, "Coprocessor Error!", User::SIGFPE)

// 对齐校验(INT 17)  *有出错码*
IMPLEMENT_EXCEPTION_ENTRANCE_ERRCODE(AlignmentCheckEntrance, AlignmentCheck)
IMPLEMENT_EXCEPTION_HANDLER_ERRCODE(AlignmentCheck, "Alignment Check!", User::SIGBUS)

// 机器检查(INT 18)
IMPLEMENT_EXCEPTION_ENTRANCE(MachineCheckEntrance, MachineCheck)
IMPLEMENT_EXCEPTION_HANDLER(MachineCheck, "Machine Check!", User::SIGNUL)

// SIMD浮点异常(INT 19)
IMPLEMENT_EXCEPTION_ENTRANCE(SIMDExceptionEntrance, SIMDException)
IMPLEMENT_EXCEPTION_HANDLER(SIMDException, "SIMD Float Point Exception!", User::SIGFPE)
