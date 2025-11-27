
//	引入需要测试的文件
#include <windows.h>

#include "stl_bits.h"
#include "stl_vector.h"
#include "stl_map.h"
#include "stl_set.h"
#include "stl_list.h"
#include "stl_queue.h"
#include "stl_unique_ptr.h"
#include "stl_shared_ptr.h"
#include "stl_thread.h"
#include "stl_string.h"

#include "stl_bind_invoke.h"
#include "stl_future.h"

#include "Coroutine.h"

#include "MemoryPool.h"
#include "ringbuffer.h"
#include "SpinLock.h"
#include "Timer.h"

#include "NetWork.h"
#include "net_asio.h"

#include "CpuInfo.h"


namespace
{
	CpuInfo cpu_info;

	STL_Bits stl_bits;
	STL_Vector stl_vector;
	STL_UniquePtr stl_uniqueptr;
	STL_SharedPtr stl_sharedptr;
	STL_Set stl_set;
	STL_Queue stl_queue;
	STL_Map stl_map;
	STL_List stl_list;
	STL_Future stl_future;
	MemoryPool memory_pool;
	RingBuffer ring_buffer;
	STL_Bind_Invoke stl_bind_invoke;
	STL_Thread stl_thread;
	STL_String stl_string;
	MS_Lock ms_lock;
	TimeDevice time_device;
	STL_Coroutine stl_coroutine;

	NetWork network;
	net_asio asio_network;
}

int main()
{
	//	修改控制台字体颜色
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    ObsMgr::instance().Run();

	std::print("按回车键退出... \n");
    std::cin.ignore(32, '\n');
}
