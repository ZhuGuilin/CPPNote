
//	引入需要测试的文件

#include "stl_vector.h"
#include "stl_map.h"
#include "stl_set.h"
#include "stl_list.h"
#include "stl_queue.h"
#include "stl_unique_ptr.h"
#include "stl_shared_ptr.h"
#include "stl_thread.h"

#include "stl_bind_invoke.h"
#include "stl_future.h"

#include "Coroutine.h"

#include "MemoryPool.h"
#include "SpinLock.h"

#include <windows.h>

namespace
{
	STL_Vector stl_vector;
	STL_UniquePtr stl_uniqueptr;
	STL_SharedPtr stl_sharedptr;
	STL_Set stl_set;
	STL_Queue stl_queue;
	STL_Map stl_map;
	STL_List stl_list;
	STL_Future stl_future;
	MemoryPool memory_pool;
	STL_Bind_Invoke stl_bind_invoke;
	//STL_Thread stl_thread;
	MS_Lock ms_lock;
	STL_Coroutine stl_coroutine;
}

int main()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    ObsMgr::instance().Run();

    std::cout << "按回车键退出... \n";
    std::cin.ignore(32, '\n');
}
