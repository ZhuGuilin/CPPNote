#pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "Observer.h"

class STL_SharedPtr : public Observer
{
public:

	class ThreadGuard
	{
		std::thread _t;

	public:

		explicit ThreadGuard(std::thread&& t) : _t(std::move(t))
		{
			if (!_t.joinable())
				throw std::logic_error("no thread!");
		}

		virtual ~ThreadGuard()
		{
			if (_t.joinable())
				_t.join();
		}

		ThreadGuard(const ThreadGuard&) = delete;
		ThreadGuard(ThreadGuard&&) = default;
		ThreadGuard& operator=(const ThreadGuard&) = delete;
		ThreadGuard& operator=(ThreadGuard&&) = delete;
	};

	class ThreadGuardDetach
	{
		std::thread _t;

	public:

		explicit ThreadGuardDetach(std::thread&& t) : _t(std::move(t))
		{
			std::cout << "Construct ThreadGuardDetach, Call detach()" << std::endl;
			if (!_t.joinable())
				throw std::logic_error("no thread!");

			_t.detach();
		}

		virtual ~ThreadGuardDetach()
		{
			if (_t.joinable())
			{
				std::cout << "Destruct ThreadGuardDetach, Call join()" << std::endl;
				_t.join();
			}
		}

		ThreadGuardDetach(const ThreadGuardDetach&) = delete;
		ThreadGuardDetach(ThreadGuardDetach&&) = default;
		ThreadGuardDetach& operator=(const ThreadGuardDetach&) = delete;
		ThreadGuardDetach& operator=(ThreadGuardDetach&&) = delete;
	};

	struct node : public std::enable_shared_from_this<node>
	{
		std::unique_ptr<int> data;
		std::string name;
		
		node(int id, std::string&& str) 
			: data(std::make_unique<int>(id)), name(std::move(str)) {}

		~node() 
		{ 
			data.reset();
			std::cout << "destruct STL_SharedPtr::node." << std::endl;
		}

		void Fn_call()
		{

#if 0
			//	lambda [&] 捕获内部成员引用，但并不能增加 shared_ptr计数
			ThreadGuardDetach t(std::thread([&]() {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1.7s);
				std::cout << "此时shared_ptr已经销毁，危险的引用 => data : " << *data << "   name : " << name << std::endl;
				}));

#else
			//	借助 shared_from_this 正确转发自身，shared_ptr生命周期被延长至 t1线程结束
			ThreadGuardDetach t1(std::thread([self = shared_from_this()]() {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1.6s);
				std::cout << "shared_ptr被安全转发进来，此时可以正常输出 => data : " << *(self->data) << "   name : " << self->name << std::endl;
				}));
#endif
		}
	};

	void Test() override
	{
		std::cout << " ===== STL_SharedPtr Bgein =====" << std::endl;

		{
			std::shared_ptr<node> ptr = std::make_shared<node>(666, std::string("SharedPtr"));
			auto ptr1 = ptr;		//	ok , 支持复制
			auto ptr2 = std::move(ptr);	//	ok , 支持移动所有权
			ptr2->Fn_call();
		}

		std::cout << " ===== STL_SharedPtr End =====" << std::endl;
	}
};
