#pragma once

#include <print>
#include <memory>
#include <functional>
//#include <vector>
#include "Observer.h"

class STL_UniquePtr : public Observer
{
public:

	struct node
	{
		int index;
		std::string name;

		node() : index(0) {}
		node(int id, std::string&& str) : index(id), name(std::move(str)) {}

		~node()
		{
			std::print("~node index : {}.\n", index);
		}

		void destroy() const noexcept
		{
			std::print("STL_UniquePtr delete function .\n");
		}
	};

	void Test() override
	{
		std::print(" ===== STL_UniquePtr Bgein =====\n");
		{
			std::unique_ptr<node> ptr = std::make_unique<node>();
			//auto ptr1 = ptr;		//	err , 不支持复制
			auto ptr2 = std::move(ptr);	//	ok , 支持移动所有权
			ptr2->name = "UniquePtr";
		}

		/*
		{
			auto ptr = std::make_unique<node, std::function<void(node*)>>({}, [](node* n) {
				if (n)
					n->destroy();
				});
		}
		*/
		{
			std::unique_ptr<node[]> ptrs = std::make_unique<node[]>(4);	//	ptrs 中存放 24个node对象
			ptrs[0].index = 1;
			ptrs[1].index = 2;
			ptrs[2].index = 3;
		}

		{
			std::vector<std::unique_ptr<node>> ptrVec;
			ptrVec.resize(32);
			ptrVec[0] = std::make_unique<node>(0, std::string("123"));
			ptrVec[0]->index = 4;
			ptrVec[0] = nullptr;
		}

		std::print(" ===== STL_UniquePtr End =====\n");
	}
};
