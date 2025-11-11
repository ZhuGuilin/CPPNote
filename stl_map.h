#pragma once

#include <print>
#include <map>
#include "Observer.h"

class STL_Map : public Observer
{
public:

	void Test() override
	{
		std::print(" ===== STL_Map Bgein =====\n");

		std::map<int, int> mp;
		mp.insert({ 1, 21 });

		std::print(" ===== STL_Map End =====\n");
	}
};
