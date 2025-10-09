#pragma once

#include <iostream>
#include <set>
#include "Observer.h"

class STL_Set : public Observer
{
public:

	void Test() override
	{
		std::cout << " ===== STL_Coroutine Bgein =====" << std::endl;

		std::set<int> set;
		set.insert({ 1 });

		std::cout << " ===== STL_Coroutine End =====" << std::endl;
	}
};
