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

		vec.reserve(128);	//	Ԥ��������ʵ�� size ��Ȼ�� 0 ������push_back������Ƶ������
		vec.push_back(7);	//	push һ��Ԫ�أ���ʱ size() = 1, capacity() = 128

		vec.clear();	//	ֻ������ݲ������ڴ棬���� _last ���� _first ����֮ǰ��������Ȼ����
		vec.shrink_to_fit();	//	�����ͷ��ڴ棨������Ϊ0��

		std::vector<int>().swap(vec);	//	���� swap ����һ���յ� vector ����ڴ棬ԭ vec û�б����ö��ͷ�

		vec.resize(64);	//	����������������ʱ size() = capacity() = 64

		std::vector<VecTestData> datas;
		VecTestData vecD1{ 1 }, vecD2{ 2 };

		datas.push_back(vecD1);		//	��Ҫ����/�ƶ� ���ж���ʱʹ��
		datas.emplace_back(vecD2);	//	���Ӷ�������ʹ�ã��������ɱ��ߣ�������������ݣ�

		datas.emplace_back(576);	//	�����Ԫ�أ�ֱ�Ӵ��빹�����������������/�ƶ� ����

		std::vector<int> intlist(1000, 1); // 1000��1

		std::cout << " ===== STL_Vector End =====" << std::endl;
	}
};
