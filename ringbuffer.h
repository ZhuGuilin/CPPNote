#pragma once

#include <print>

#include "observer.h"


class RingBuffer : public Observer
{
public:



	void Test() override
	{
		std::print(" ===== RingBuffer Bgein =====\n");



		std::print(" ===== RingBuffer End =====\n");
	}
};