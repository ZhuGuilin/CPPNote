#pragma once

#include <print>
#include <memory>
#include <functional>
//#include <vector>
#include "Observer.h"

#include "MemoryPool.h"

class STL_UniquePtr : public Observer
{
public:

	//	copy form trinitycore
	template<typename T, typename Del>
	struct stateful_unique_ptr_deleter
	{
		using pointer = T;
		explicit(false) stateful_unique_ptr_deleter(Del deleter) : _deleter(std::move(deleter)) {}
		void operator()(pointer ptr) const { (void)_deleter(ptr); }
		//void operator()(pointer ptr) const noexcept { (noexcept(_deleter(ptr))); }

	private:
		Del _deleter;
	};

	template<typename T, auto Del>
	struct stateless_unique_ptr_deleter
	{
		using pointer = T;
		void operator()(pointer ptr) const
		{
			if constexpr (std::is_member_function_pointer_v<decltype(Del)>)
				(void)(ptr->*Del)();
			else
				(void)Del(ptr);
		}
	};

	/**
	 * Convenience function to construct type aliases for std::unique_ptr stateful deleters (such as lambda with captures)
	 * @tparam Ptr    Type of the pointer
	 * @tparam Del    Type of object freeing function (deduced from argument)
	 * @param deleter Object deleter
	 *
	 * Example use
	 * @code
	 * void FreeV1(Resource*);
	 * void FreeV2(Resource*);
	 *
	 * using ResourceDeleter = decltype(Trinity::unique_ptr_deleter<Resource*>(&FreeV1));
	 *
	 * std::unique_ptr<Resource, ResourceDeleter> resource = Trinity::make_unique_ptr_with_deleter(GetResourceV1(), &FreeV1);
	 * // do stuff ...
	 * // ... later get new resource
	 * resource = Trinity::make_unique_ptr_with_deleter(GetResourceV2(), &FreeV2);
	 * @endcode
	 */
	template <typename Ptr, typename Del> requires std::invocable<Del, Ptr>&& std::is_pointer_v<Ptr>
	stateful_unique_ptr_deleter<Ptr, Del> unique_ptr_deleter(Del deleter)
	{
		return stateful_unique_ptr_deleter<Ptr, Del>(std::move(deleter));
	}

	/**
	 * Convenience function to construct type aliases for std::unique_ptr stateful deleters (such as lambda with captures)
	 *
	 * Main use is for forming struct/class members without the call to make_unique_ptr_with_deleter
	 * @tparam Ptr    Type of the pointer
	 * @tparam Del    The freeing function. This can be either a free function pointer that accepts T* as argument, pointer to a member function of T that accepts no arguments or a lambda with no captures
	 *
	 * Example use
	 * @code
	 * using FileDeleter = decltype(Trinity::unique_ptr_deleter<FILE*, &::fclose>());
	 *
	 * class Resource
	 * {
	 *     std::unique_ptr<FILE, FileDeleter> File;
	 * };
	 * @endcode
	 */
	template <typename Ptr, auto Del> requires std::invocable<decltype(Del), Ptr>&& std::is_pointer_v<Ptr>
	stateless_unique_ptr_deleter<Ptr, Del> unique_ptr_deleter()
	{
		return stateless_unique_ptr_deleter<Ptr, Del>();
	}

	/**
	 * Utility function to construct a std::unique_ptr object with custom stateful deleter (such as lambda with captures)
	 * @tparam Ptr    Type of the pointer
	 * @tparam T      Type of the pointed-to object (defaults to std::remove_pointer_t<Ptr>)
	 * @tparam Del    Type of object freeing function (deduced from second argument)
	 * @param ptr     Raw pointer to owned object
	 * @param deleter Object deleter
	 *
	 * Example use
	 * @code
	 * class Resource
	 * {
	 * };
	 * class ResourceService
	 * {
	 *     Resource* Create();
	 *     void Destroy(Resource*);
	 * };
	 *
	 * ResourceService* service = GetResourceService();
	 *
	 * // Lambda that captures all required variables for destruction
	 * auto resource = Trinity::make_unique_ptr_with_deleter(service->Create(), [service](Resource* res){ service->Destroy(res); });
	 * @endcode
	 */
	template<typename Ptr, typename T = std::remove_pointer_t<Ptr>, typename Del> 
		requires std::invocable<Del, Ptr>&& std::is_pointer_v<Ptr>
	std::unique_ptr<T, stateful_unique_ptr_deleter<Ptr, Del>> make_unique_ptr_with_deleter(Ptr ptr, Del deleter)
	{
		return std::unique_ptr<T, stateful_unique_ptr_deleter<Ptr, Del>>(ptr, std::move(deleter));
	}

	/**
	 * Utility function to construct a std::unique_ptr object with custom stateless deleter (function pointer, captureless lambda)
	 * @tparam Del    The freeing function. This can be either a free function pointer that accepts T* as argument, pointer to a member function of T that accepts no arguments or a lambda with no captures
	 * @tparam Ptr    Type of the pointer
	 * @tparam T      Type of the pointed-to object (defaults to std::remove_pointer_t<Ptr>)
	 * @param ptr     Raw pointer to owned object
	 *
	 * Example uses
	 * @code
	 * void DestroyResource(Resource*);
	 * class Resource
	 * {
	 *     void Destroy();
	 * };
	 *
	 * // Free function
	 * auto free = Trinity::make_unique_ptr_with_deleter<&DestroyResource>(new Resource());
	 *
	 * // Member function
	 * auto member = Trinity::make_unique_ptr_with_deleter<&Resource::Destroy>(new Resource());
	 *
	 * // Lambda
	 * auto lambda = Trinity::make_unique_ptr_with_deleter<[](Resource* res){ res->Destroy(); }>(new Resource());
	 * @endcode
	 */
	template<auto Del, typename Ptr, typename T = std::remove_pointer_t<Ptr>> 
		requires std::invocable<decltype(Del), Ptr>&& std::is_pointer_v<Ptr>
	std::unique_ptr<T, stateless_unique_ptr_deleter<Ptr, Del>> make_unique_ptr_with_deleter(Ptr ptr)
	{
		return std::unique_ptr<T, stateless_unique_ptr_deleter<Ptr, Del>>(ptr);
	}

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

		MemoryPool::SimpleMemoryPool<node> smpool;
		{
			auto ptr = make_unique_ptr_with_deleter(smpool.Alloc(), [&smpool](node* p) {
				smpool.Recycle(p);
				});

			ptr->index = 12999;
			ptr->name = "haha";
		}
		

		std::print(" ===== STL_UniquePtr End =====\n");
	}
};
