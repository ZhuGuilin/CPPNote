#pragma once

#include <print>
#include <bit>

#include "Observer.h"


class STL_Bits : public Observer
{
public:


	void Test() override
	{
		std::println("===== STL_Bits Bgein =====");

		//	查找第一个1位置
		auto v = 0b00110110u;
		std::println("std::countr_zero : {}", std::countr_zero(v));
		

		std::println("===== STL_Bits End =====");
	}

};