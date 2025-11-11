#pragma once

#include <print>
#include <set>
#include "Observer.h"

class STL_Set : public Observer
{
public:

	void Test() override
	{
		std::print(" ===== STL_Coroutine Bgein =====\n");

		std::set<int> set;
		set.insert({ 1 });

		std::print(" ===== STL_Coroutine End =====\n");
	}
};
