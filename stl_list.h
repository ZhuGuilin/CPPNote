#pragma once

#include <print>
#include <list>
#include "Observer.h"

class STL_List : public Observer
{
public:

	struct node
	{
		int  key;
		int  version;
		std::string message;
	};

	void Test() override
	{
		std::print(" ===== STL_List Bgein =====\n");

		std::list<node> node_list;
		node_list.push_back({ 1, 0, "hi list"});

		std::print(" ===== STL_List End =====\n");
	}
};
