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

		//	std::queue 先进先出队列
		std::queue<node> node_que;
		node_que.push({ 1 });
		node_que.push({ 9 });
		node_que.push({ 0 });
		auto& v = node_que.front();
		v.data += 10;
		node_que.pop();

		std::print(" ===== STL_Queue End =====\n");
	}
};
