#pragma once

//#include <print>
//#include <flat_map>
//#include "Observer.h"
#include "MemoryPool.h"

#include <map>
#include <unordered_map>


template<typename Key, typename Value, typename Hash = std::hash<Key>>
class ConcurrentHashMap {
public:



private:

	static constexpr float kDefaultLoadFactor = 1.05f;

	class alignas(64) Segment {
	public:

		Segment() {}
		~Segment() = default;
		Segment(const Segment&) = delete;
		Segment(Segment&&) = delete;
		Segment& operator=(const Segment&) = delete;
		Segment& operator=(Segment&&) = delete;

	private:

		struct Node {
			using value_type = std::pair<const Key, Value>;

			explicit Node(const Node& other) : item(other.item) {}

			template <typename Arg, typename... Args>
			Node(Arg&& k, Args&&... args)
				: item(std::piecewise_construct,
					std::forward_as_tuple(std::forward<Arg>(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)) {
			}

			std::atomic<Node*> next{ nullptr };
			value_type item;
		};

		std::mutex	_mutex;
		std::atomic<std::uint64_t> _size;
		const std::uint64_t _capacity;
		MemoryPool::SimpleMemoryPool<Node> _pool;
	};
};

class STL_Map : public Observer
{
public:
	

	void Test() override
	{
		std::print(" ===== STL_Map Bgein =====\n");

		std::map<int, int> mp;
		mp.insert({ 1, 21 });

		std::unordered_map<std::string, std::string> hashmap;
		hashmap.insert({ "123", "lkdfjlskj33478798" });

		//	std::flat_map 在 windows还未支持？无法引入<flat_map>头文件
		//	频繁查找和遍历为主,内部实现与std::map的红黑树不一样，是连续内存存储
		//  频繁插入或删除性能较慢，这时优先选择std::unordered_map或者std::map
		//  查找速度比标准关联容器std::map更快。
		//	迭代速度比标准关联容器std::map快得多。
		//	使用随机访问迭代器而不是双向迭代器。
		//	每个元素的内存消耗更少。
		//	改进的缓存性能（数据存储在连续内存中）。
		//	迭代器不稳定（插入和删除元素时迭代器会失效）。
		//	无法存储不可复制和不可移动的值类型。
		//	异常安全性比标准关联容器弱（在删除和插入时移动值可能会抛出异常）。
		//	插入和删除操作比标准关联容器慢（尤其是对于不可移动类型）
		//std::flat_map<int, int> flatmap;
		//flatmap.insert({ 2, 35 });

		std::print(" ===== STL_Map End =====\n");
	}
};
