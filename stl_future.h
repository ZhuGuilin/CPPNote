#pragma once

#include <print>
#include <memory>
#include <functional>
#include <future>

#include "Observer.h"

class STL_Future : public Observer
{
public:

	void Test() override
	{
		std::print(" ===== STL_Future Bgein =====\n");


		std::print(" ===== STL_Future End =====\n");
	}

};

/*
std::future 是现代C++异步编程的基石：

核心价值：分离任务执行和结果获取
关键特性：阻塞等待、结果检索、异常传播

适用场景：
	线程池任务提交
	并行算法实现
	异步I/O操作
	事件驱动架构

在实际使用中：
	优先使用 std::async 创建简单异步任务
	在线程池中使用 std::packaged_task + future
	多线程等待使用 std::shared_future
	警惕生命周期问题和有效性状态
	结合C++20协程实现更优雅的异步代码

	但是协程并不太优雅，一想到就头大
*/