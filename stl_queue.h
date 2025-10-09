#pragma once

#include <iostream>
#include <queue>
#include "Observer.h"

class STL_Queue : public Observer
{
public:

	struct node
	{
		int data;
	};

	void Test() override
	{
		std::cout << " ===== STL_Queue Bgein =====" << std::endl;

		std::queue<node> node_que;
		node_que.push({1});

		std::cout << " ===== STL_Queue End =====" << std::endl;
	}
};
