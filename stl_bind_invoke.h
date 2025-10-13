#pragma once

#include <iostream>
#include <memory>
#include <functional>

#include "Observer.h"

namespace {
//	copy from folly invoke.h
struct invoke_fn {
	template <typename F, typename... A>
	inline constexpr auto operator()(F&& f, A&&... a) const
		noexcept(noexcept(static_cast<F&&>(f)(static_cast<A&&>(a)...)))
		-> decltype(static_cast<F&&>(f)(static_cast<A&&>(a)...)) {
		return static_cast<F&&>(f)(static_cast<A&&>(a)...);
	}

	template <typename M, typename C, typename... A>
	inline constexpr auto operator()(M C::* f, A&&... a) const
		noexcept(noexcept(std::mem_fn(f)(static_cast<A&&>(a)...)))
		-> decltype(std::mem_fn(f)(static_cast<A&&>(a)...)) {
		return std::mem_fn(f)(static_cast<A&&>(a)...);
	}
};
inline constexpr invoke_fn invoke;

template<typename F, typename... A>
void caller_bind(F&& f, A&&... a) {
	auto task = std::bind(std::forward<F>(f), std::forward<A>(a)...);
	task();
}

template<typename F, typename... A>
inline constexpr auto caller_invoke(F&& f, A&&... a) {
	std::invoke(std::forward<F>(f), std::forward<A>(a)...);
}

std::string CallTest(int id, std::string& str)
{
	str += " CallTest => id : " + std::to_string(id);
	return str;
}

void Xtest(int id, uint32_t& res)
{
	res += (id & 0xFFFF);
}

struct Fa
{
	std::string InvokeTest(int id, std::string& str)
	{
		str += " InvokeTest => id : " + std::to_string(id);
		return str;
	}

	void Ytest(int id, uint32_t& res)
	{
		res += (id & 0xFFFF);
	}
};

}

class STL_Bind_Invoke : public Observer
{
public:

	void Test() override
	{
		std::cout << " ===== STL_Bind_Invoke Bgein =====" << std::endl;
		Fa fa;
		(void)fa;	//	�������������
#if 0
		caller_bind(printf, " caller_bind \n");
		caller_invoke(printf, " caller_invoke \n");

		std::string ins(" invoke_fn |");
		std::cout << invoke(CallTest, 1002, ins) << std::endl;
		std::cout << invoke(&Fa::InvokeTest, &fa, 1002, ins) << std::endl;
		caller_invoke(&Fa::InvokeTest, &fa, 1002, ins);
		std::cout << invoke([](int id, std::string& str) -> std::string {
			str += " CallTest lamda => id : " + std::to_string(id);
			return str;
			}, 1002, ins) << std::endl;
#endif

#if 0
		uint32_t res{0};
		auto start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 50000000; i++)
		{ 
			//invoke(Xtest, i, res);
			invoke(&Fa::Ytest, &fa, i, res);
		}
		auto end = std::chrono::high_resolution_clock::now();
		std::cout << "invoke ��ʱ: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			<< " ms" << "   res :" << res << std::endl;

		start = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < 50000000; i++)
		{
			//caller_invoke(Xtest, i, res);
			caller_invoke(&Fa::Ytest, &fa, i, res);
		}
		end = std::chrono::high_resolution_clock::now();
		std::cout << "caller_invoke ��ʱ: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			<< " ms" << "   res :" << res << std::endl;

		//	���Խ���� ʹ�� invoke �� caller_invoke �����ܲ���
#endif

		std::cout << " ===== STL_Bind_Invoke End =====" << std::endl;
	}

};
