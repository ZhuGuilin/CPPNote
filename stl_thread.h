#pragma once


#include <print>
#include <thread>
#include <chrono>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <type_traits>

#include "Observer.h"

thread_local int thread_specific = 0;	// 每个线程独立副本
void worker1(int id, std::string& str)
{
	std::print("worker1 args => id : {}, str : {}.\n", id, str);
}

void worker2()
{
	for (int i = 0; i < 100; i++)
		++thread_specific;

	std::print("worker2 tid : {}, thread_specific : {}.\n", std::this_thread::get_id(), thread_specific);
}

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

class STL_Thread : public Observer
{
public:

	class ThreadManager		//	简单线程创建的测试
	{
		std::atomic_bool _stop{ false };
		std::vector<ThreadGuardJoin> _threads;
		//std::vector<ThreadGuardDetach> _threads;

		ThreadManager()
		{
			//	通过配置获取线程数量
			//	...
			using namespace std::chrono_literals;
			const int num_cpu = max(1u, std::thread::hardware_concurrency()) / 2;
			_threads.reserve(num_cpu);
			for (int i = 0; i < num_cpu; i++)
			{
				_threads.emplace_back(std::thread([&]() {
					while (!_stop.load())
					{
						std::print("ThreadManager working tid ：{}.\n", std::this_thread::get_id());
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

	class ThreadPool	//	线程池简单功能实现（不含调度策略、负载均衡等设计）
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
				std::print("ThreadPool Stop _tasks size : {}.\n", _tasks.size());
		}

		//	提交任务
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
				_tasks.emplace([task]() { (*task)(); }); // 添加任务到队列
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
			//	通过配置获取线程数量
			//	...
			using namespace std::chrono_literals;
			const int num_cpu = max(1u, std::thread::hardware_concurrency()) / 2;
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
							std::print("ThreadPool task exception : {}.\n", e.what());
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
		std::queue<Task> _tasks;	//	这里应该使用线程安全的队列
	};

	void Test() override
	{
		using namespace std::chrono_literals;
		std::print(" ===== STL_Thread Bgein =====\n");

		std::print("cpu 逻辑处理器数量 ：{}.\n", std::thread::hardware_concurrency());
		std::print("当前线程 Main Thread ID ：{}.\n", std::this_thread::get_id());

		std::this_thread::yield();	// 让出CPU

		int num = 0;
		std::string str = "abcdef";
		std::thread t(worker1,
			num,       // 值传递（复制）
			std::ref(str) // 引用传递（需显式使用 std::ref）
		);

		t.join();		// worker1已经在运行中，阻塞当前线程直到 t 完成

		//t.detach();		// 线程独立运行，资源自动回收
		//assert(!t.joinable());

		/*
		特性		   join()		detach()
		阻塞行为|  阻塞调用线程	立即返回
		线程状态|  等待结束		后台运行
		资源回收|  join时回收	自动回收
		结果获取|  可直接获取	需其他机制
		使用风险|  死锁风险		悬挂引用风险
		适用场景|  需要同步		独立任务
		*/
#if 0
		//	平台原生句柄
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
		std::this_thread::sleep_for(1s);		// 休眠 2s		ms us ns
		ThreadManager::instance().Stop();

		ThreadPool::instance();
		std::string msg = "***Processing  ";
		auto taskres = ThreadPool::instance().Submit([&msg](int id) {
			msg += "task =>  " + std::to_string(id);
			std::this_thread::sleep_for(1s);
			return msg;
			}, 1001);

		std::print("ThreadPool test result : {}.\n", taskres.get());	//	get() 阻塞等待执行结果
		ThreadPool::instance().Stop();

		std::print(" ===== STL_Thread End =====\n");
	}
};