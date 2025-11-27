#pragma once

#include <print>
#include <set>
//#include <flat_set>
#include "Observer.h"

class STL_Set : public Observer
{
public:

	void Test() override
	{
		std::print(" ===== STL_Coroutine Bgein =====\n");

		std::set<int> set;
		set.insert({ 1 });

		//	”Îstd::flat_map¿‡À∆
		//std::flat_set<int> flatset;
		//flatset.insert({ 2 });

		std::print(" ===== STL_Coroutine End =====\n");
	}
};
