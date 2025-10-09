#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <coroutine>

#include "Observer.h"

class STL_Coroutine : public Observer
{
public:

	

	void Test() override
	{
		std::cout << " ===== STL_Coroutine Bgein =====" << std::endl;


		std::cout << " ===== STL_Coroutine End =====" << std::endl;
	}

};

/*
协程是一种可暂停和恢复的函数:

	可以在执行过程中暂停，保存当前状态
	稍后从暂停点恢复执行
	保留所有局部变量状态
	不需要操作系统线程切换

特性			传统函数				协程
执行流程	|	一次执行完成			可分多次执行
状态保存	|	无（局部变量销毁）	保留全部局部状态
调用开销	|	栈帧创建/销毁			初始开销大，恢复开销小
并发模型	|	依赖多线程			单线程可管理数千协程

协程核心组件:
	协程句柄：std::coroutine_handle - 控制协程生命周期
	承诺对象：promise_type - 定制协程行为
	协程帧：编译器生成的隐藏数据结构，存储协程状态

promise_type：必须包含的嵌套类型
	get_return_object()：创建协程的返回值对象
	initial_suspend()：初始挂起策略
	final_suspend()：最终挂起策略
	return_void()：处理co_return
	unhandled_exception()：异常处理

挂起类型：
	std::suspend_always：总是挂起
	std::suspend_never：从不挂起

协程生命周期：
	创建：调用协程函数时
	挂起：co_await或co_yield
	恢复：handle.resume()
	销毁：handle.destroy()

协程三大关键字
	1. co_await - 异步等待
	2. co_yield - 生成值
	3. co_return - 返回值
*/