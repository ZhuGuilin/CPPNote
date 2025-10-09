#pragma once

#include <iostream>
#include <vector>
#include "Observer.h"

class STL_Vector : public Observer
{
public:

	struct VecTestData
	{
		VecTestData(int id) : index(id), data(0)
		{

		}

		~VecTestData()
		{

		}

		VecTestData(const VecTestData& r)
		{
			index = r.index;
			data = r.data;
			std::cout << "VecTestData(const VecTestData&) id : " << index << std::endl;
		}

		VecTestData(VecTestData&& r) noexcept
		{
			index = r.index;
			data = r.data;
			std::cout << "VecTestData(VecTestData&&) id : " << index << std::endl;
		}
		VecTestData& operator=(VecTestData const& r)
		{
			index = r.index;
			data = r.data;
			std::cout << "VecTestData& operator=(VecTestData const&) id : " << index << std::endl;
		}
		VecTestData& operator=(VecTestData&& r) noexcept
		{
			index = r.index;
			data = r.data;
			std::cout << "VecTestData& operator=(VecTestData&&) id : " << index << std::endl;
		}

		int index;
		uint32_t data;
	};

	void Test() override
	{
		std::cout << " ===== STL_Vector Bgein =====" << std::endl;

		std::vector<int> vec;

		vec.reserve(128);	//	预设容量，实际 size 仍然是 0 ，避免push_back过程中频繁扩容
		vec.push_back(7);	//	push 一个元素，此时 size() = 1, capacity() = 128

		vec.clear();	//	只清空内容不回收内存，仅将 _last 移至 _first 处，之前的数据仍然保留
		vec.shrink_to_fit();	//	请求释放内存（容量降为0）

		std::vector<int>().swap(vec);	//	利用 swap 交换一个空的 vector 清除内存，原 vec 没有被引用而释放

		vec.resize(64);	//	重新设置容量，此时 size() = capacity() = 64

		std::vector<VecTestData> datas;
		VecTestData vecD1{ 1 }, vecD2{ 2 };

		datas.push_back(vecD1);		//	需要复制/移动 现有对象时使用
		datas.emplace_back(vecD2);	//	复杂对象优先使用，如对象构造成本高（如包含大量数据）

		datas.emplace_back(576);	//	添加新元素，直接传入构造参数，不触发拷贝/移动 构造

		std::vector<int> intlist(1000, 1); // 1000个1

		std::cout << " ===== STL_Vector End =====" << std::endl;
	}
};
