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
			//	lambda [&] �����ڲ���Ա���ã������������� shared_ptr����
			ThreadGuardDetach t(std::thread([&]() {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1.7s);
				std::cout << "��ʱshared_ptr�Ѿ����٣�Σ�յ����� => data : " << *data << "   name : " << name << std::endl;
				}));

#else
			//	���� shared_from_this ��ȷת������shared_ptr�������ڱ��ӳ��� t1�߳̽���
			ThreadGuardDetach t1(std::thread([self = shared_from_this()]() {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1.6s);
				std::cout << "shared_ptr����ȫת����������ʱ����������� => data : " << *(self->data) << "   name : " << self->name << std::endl;
				}));
#endif
		}
	};

	void Test() override
	{
		std::cout << " ===== STL_SharedPtr Bgein =====" << std::endl;

		{
			std::shared_ptr<node> ptr = std::make_shared<node>(666, std::string("SharedPtr"));
			auto ptr1 = ptr;		//	ok , ֧�ָ���
			auto ptr2 = std::move(ptr);	//	ok , ֧���ƶ�����Ȩ
			ptr2->Fn_call();
		}

		std::cout << " ===== STL_SharedPtr End =====" << std::endl;
	}
};
