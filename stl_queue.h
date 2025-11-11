#pragma once

#include <print>
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
		std::print(" ===== STL_Queue Bgein =====\n");

		std::queue<node> node_que;
		node_que.push({1});

		std::print(" ===== STL_Queue End =====\n");
	}
};
