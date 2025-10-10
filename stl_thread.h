#pragma once


#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <type_traits>

#include "Observer.h"

thread_local int thread_specific = 0;	// ÿ���̶߳�������
void worker1(int id, std::string& str)
{
	std::cout << "worker1 args => id : " << id << "   str : " << str << std::endl;
}

void worker2()
{
	for (int i = 0; i < 100; i++)
		++thread_specific;

	std::cout << "worker2 tid : " << std::this_thread::get_id() << "  thread_specific : " << thread_specific << std::endl;
}

class STL_Thread : public Observer
{
public:

	class ThreadGuardJoin
	{
	public:

		explicit ThreadGuardJoin(std::thread&& t) noexcept(false)
			: _t(std::move(t))
		{
			if (!_t.joinable())
				throw std::logic_error("no thread!");
		}

		ThreadGuardJoin(ThreadGuardJoin&& other) noexcept
			: _t(std::move(other._t))
		{
		}

		virtual ~ThreadGuardJoin()
		{
			if (_t.joinable())
				_t.join();
		}

		ThreadGuardJoin(const ThreadGuardJoin&) = delete;
		ThreadGuardJoin& operator=(const ThreadGuardJoin&) = delete;
		ThreadGuardJoin& operator=(ThreadGuardJoin&& other) = delete;

	private:

		std::thread _t;
	};

	class ThreadGuardDetach
	{
	public:

		explicit ThreadGuardDetach(std::thread&& t) noexcept(false)
			: _t(std::move(t))
		{
			std::cout << "Construct ThreadGuardDetach, Call detach()" << std::endl;
			if (!_t.joinable())
				throw std::logic_error("no thread!");

			_t.detach();
		}

		~ThreadGuardDetach() = default;
		ThreadGuardDetach(const ThreadGuardDetach&) = delete;
		ThreadGuardDetach(ThreadGuardDetach&&) noexcept = default;
		ThreadGuardDetach& operator=(const ThreadGuardDetach&) = delete;
		ThreadGuardDetach& operator=(ThreadGuardDetach&&) = delete;

	private:

		std::thread _t;
	};

	class ThreadManager		//	���̴߳����Ĳ���
	{
		std::atomic_bool _stop{ false };
		std::vector<ThreadGuardJoin> _threads;
		//std::vector<ThreadGuardDetach> _threads;

		ThreadManager()
		{
			//	ͨ�����û�ȡ�߳�����
			//	...
			using namespace std::chrono_literals;
			const int num_cpu = std::max(1u, std::thread::hardware_concurrency()) / 2;
			_threads.reserve(num_cpu);
			for (int i = 0; i < num_cpu; i++)
			{
				_threads.emplace_back(std::thread([&]() {
					while (!_stop.load())
					{
						std::cout << "ThreadManager working tid ��" << std::this_thread::get_id() << std::endl;
						std::this_thread::sleep_for(300ms);
					}
					}));
			}
		}

		~ThreadManager() { _threads.clear(); }

	public:

		inline static ThreadManager& instance() noexcept
		{
			static ThreadManager sThreadManager;
			return sThreadManager;
		}

		inline void Stop() noexcept
		{
			_stop.store(true);
		}

		ThreadManager(const ThreadManager&) = delete;
		ThreadManager(ThreadManager&&) = delete;
		ThreadManager& operator=(const ThreadManager&) = delete;
		ThreadManager& operator=(ThreadManager&&) = delete;
	};

	class ThreadPool	//	�̳߳ؼ򵥹���ʵ�֣��������Ȳ��ԡ����ؾ������ƣ�
	{
	public:

		using Task = std::function<void()>;

		inline static ThreadPool& instance() noexcept
		{
			static ThreadPool sThreadPool;
			return sThreadPool;
		}

		void Stop()
		{
			_stop.store(true);
			_condition.notify_all();
			_threads.clear();

			if (!_tasks.empty())
				std::cout << "ThreadPool Stop _tasks size��" << _tasks.size() << std::endl;
		}

		//	�ύ����
		template<typename F, typename... Args>
		auto Submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
		{
			using ResultT = std::invoke_result_t<F, Args...>;
			auto task = std::make_shared<std::packaged_task<ResultT()>>(
				[func = std::forward<F>(f), ... captured_args = std::forward<Args>(args)]() {
					return std::invoke(func, captured_args...);
				});

			std::future<ResultT> result = task->get_future();
			{
				std::lock_guard<std::mutex> lock(_mutex);
				_tasks.emplace([task]() { (*task)(); }); // ������񵽶���
			}

			_condition.notify_one();
			return result;
		}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool& operator=(ThreadPool&&) = delete;

	private:

		ThreadPool()
		{
			//	ͨ�����û�ȡ�߳�����
			//	...
			using namespace std::chrono_literals;
			const int num_cpu = std::max(1u, std::thread::hardware_concurrency()) / 2;
			_threads.reserve(num_cpu);
			for (int i = 0; i < num_cpu; i++)
			{
				_threads.emplace_back(std::thread([this]() {
					while (true)
					{
						Task task;
						{
							std::unique_lock<std::mutex> lock(_mutex);
							_condition.wait(lock, [this]() {
								return _stop.load() || !_tasks.empty();
								});

							if (_stop.load())
								return;

							task = std::move(_tasks.front());
							_tasks.pop();
						}

						try {
							task();
						}
						catch (const std::exception& e) {
							std::cerr << "ThreadPool task exception: " << e.what() << std::endl;
						}
					}
					}));
			}
		}

		~ThreadPool()
		{
			if (!_stop.load())
				Stop();
		}

		std::atomic_bool _stop{ false };
		std::vector<ThreadGuardJoin> _threads;

		std::mutex _mutex;
		std::condition_variable _condition;
		std::queue<Task> _tasks;	//	����Ӧ��ʹ���̰߳�ȫ�Ķ���
	};

	void Test() override
	{
		using namespace std::chrono_literals;
		std::cout << " ===== STL_Thread Bgein =====" << std::endl;

		std::cout << "cpu �߼����������� ��" << std::thread::hardware_concurrency() << std::endl;
		std::cout << "��ǰ�߳� Main Thread ID ��" << std::this_thread::get_id() << std::endl;

		std::this_thread::yield();	// �ó�CPU

		int num = 0;
		std::string str = "abcdef";
		std::thread t(worker1,
			num,       // ֵ���ݣ����ƣ�
			std::ref(str) // ���ô��ݣ�����ʽʹ�� std::ref��
		);

		t.join();		// worker1�Ѿ��������У�������ǰ�߳�ֱ�� t ���

		//t.detach();		// �̶߳������У���Դ�Զ�����
		//assert(!t.joinable());

		/*
		����		   join()		detach()
		������Ϊ|  ���������߳�	��������
		�߳�״̬|  �ȴ�����		��̨����
		��Դ����|  joinʱ����	    �Զ�����
		�����ȡ|  ��ֱ�ӻ�ȡ	    ����������
		ʹ�÷���|  ��������		�������÷���
		���ó���|  ��Ҫͬ��		��������
		*/
#if 0
		//	ƽ̨ԭ�����
#ifdef WIN32
		HANDLE h = t.native_handle();
		::SetThreadPriority(h, THREAD_PRIORITY_HIGHEST);
#else
		pthread_t tid = t.native_handle();
		pthread_setname_np(tid, "worker");
#endif // WIN32

#endif

		std::thread t2(worker2);
		std::thread t3(worker2);
		t2.join();
		t3.join();

		ThreadManager::instance();
		std::this_thread::sleep_for(1s);		// ���� 2s		ms us ns
		ThreadManager::instance().Stop();

		ThreadPool::instance();
		std::string msg = "Processing  ";
		auto taskres = ThreadPool::instance().Submit([&msg](int id) {
			msg += "task =>  " + std::to_string(id);
			std::this_thread::sleep_for(1s);
			return msg;
			}, 1001);

		std::cout << "ThreadPool test result : " << taskres.get() << std::endl;	//	get() �����ȴ�ִ�н��
		ThreadPool::instance().Stop();

		std::cout << " ===== STL_Thread End =====" << std::endl;
	}
};