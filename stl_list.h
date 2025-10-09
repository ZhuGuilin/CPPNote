#pragma once

#include <iostream>
#include <list>
#include "Observer.h"

class STL_List : public Observer
{
public:

	struct node
	{
		int data;
	};

	void Test() override
	{
		std::cout << " ===== STL_List Bgein =====" << std::endl;

		std::list<node> node_list;
		node_list.push_back({ 1 });

		std::cout << " ===== STL_List End =====" << std::endl;
	}
};
