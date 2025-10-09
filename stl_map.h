#pragma once

#include <iostream>
#include <map>
#include "Observer.h"

class STL_Map : public Observer
{
public:

	void Test() override
	{
		std::cout << " ===== STL_Map Bgein =====" << std::endl;

		std::map<int, int> mp;
		mp.insert({ 1, 21 });

		std::cout << " ===== STL_Map End =====" << std::endl;
	}
};
