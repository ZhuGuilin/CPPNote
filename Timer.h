#pragma once

#include "Observer.h"
#include <chrono>


class TimeDevice : public Observer
{
public:

	class Timer
	{
	public:

		inline static Timer& instance() noexcept
		{
			static Timer sTimer;
			return sTimer;
		}

	private:

		Timer() = default;
		~Timer() = default;
		Timer(Timer const&) = delete;
		Timer(Timer&&) = delete;
		Timer& operator=(Timer const&) = delete;
		Timer& operator=(Timer&&) = delete;


	};

	void Test() override
	{
		std::chrono::nanoseconds kMinYieldingSleep = std::chrono::microseconds(500);

	}
};