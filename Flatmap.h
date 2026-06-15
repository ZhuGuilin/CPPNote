/*
* 线性map，底层使用一个有序的vector来存储键值对。
* 适用于小规模数据的场景，具有较低的内存开销和较快的访问速度。
* 由于缓存局部性原理，FlatMap即使查找数据时可能比传统的基于树或哈希表的map更快。
* 最佳使用场景：频繁访问但不频繁修改的数据结构，如配置项、常量映射等。
*/

#ifndef __CXXNOTE_FLATMAP_H__
#define __CXXNOTE_FLATMAP_H__

#include <vector>
#include <algorithm>


namespace CxxNote
{

struct DefaultCompare
{
	template<typename T>
	bool operator()(const T& a, const T& b) const noexcept
	{
		return a < b;
	}
};

template<typename K, typename V, class Compare = DefaultCompare>
class FlatMap
{
public:

	using KeyType = K;
	using ValueType = V;
	using PairType = std::pair<K, V>;
	using ContainerType = std::vector<PairType>;
	using Iterator = typename ContainerType::iterator;
	using ConstIterator = typename ContainerType::const_iterator;

	explicit FlatMap(Compare compare = {})
		: _compare(std::move(compare)) {}

	template<typename InputIt>
	FlatMap(InputIt first, InputIt last)
		: _compare()
	{
		_data.assign(first, last);
		std::sort(_data.begin(), _data.end(),
			[this](const PairType& a, const PairType& b) {
				return _compare(a.first, b.first);
			});
	}

	template<typename InputIt>
	FlatMap(InputIt first, InputIt last, Compare compare = {})
		: _compare(std::move(compare))
	{
		_data.assign(first, last);
		std::sort(_data.begin(), _data.end(),
			[this](const PairType& a, const PairType& b) {
				return _compare(a.first, b.first);
			});
	}

	~FlatMap() = default;

	FlatMap(const FlatMap& other) = default;
	FlatMap(FlatMap&& other) noexcept = default;
	FlatMap& operator=(const FlatMap& other) = default;
	FlatMap& operator=(FlatMap&& other) noexcept = default;

	//	查找键对应的值，返回一个迭代器
	Iterator find(const KeyType& key)
	{
		auto it = std::lower_bound(_data.begin(), _data.end(), key,
			[this](const PairType& pair, const K& k) { return _compare(pair.first, k); });

		if (it != _data.end() && it->first == key)
		{
			return it;
		}

		return _data.end();
	}

	//	插入或更新键值对
	std::pair<Iterator, bool> insert(const KeyType& key, const ValueType& value)
	{
		auto it = std::lower_bound(_data.begin(), _data.end(), key,
			[this](const PairType& pair, const K& k) { return _compare(pair.first, k); });
		if (it != _data.end() && it->first == key)
		{
			it->second = value; // 更新现有键的值
			return { it, false };
		}
		else
		{
			return { _data.insert(it, { key, value }), true };
		}
	}

	std::pair<Iterator, bool> insert(const PairType& kv)
	{
		auto it = std::lower_bound(_data.begin(), _data.end(), kv.first,
			[this](const PairType& pair, const K& k) { return _compare(pair.first, k); });
		if (it != _data.end() && it->first == kv.first)
		{
			it->second = kv.second; // 更新现有键的值
			return { it, false };
		}
		else
		{
			return { _data.insert(it, kv), true };
		}
	}

	//	原位构造插入键值对，避免不必要的复制
	template<typename... Args>
	std::pair<Iterator, bool> emplace(const KeyType& key, Args&&... args)
	{
		auto it = std::lower_bound(_data.begin(), _data.end(), key,
			[this](const PairType& pair, const K& k) { return _compare(pair.first, k); });
		if (it != _data.end() && it->first == key) [[unlikely]]
		{
			return { it, false };
		}

		return { _data.emplace(it, std::piecewise_construct,
					std::forward_as_tuple(key),
					std::forward_as_tuple(std::forward<Args>(args)...)), true };
	}

	//	删除键值对，返回是否成功删除
	bool erase(const KeyType& key)
	{
		auto it = std::lower_bound(_data.begin(), _data.end(), key,
			[this](const PairType& pair, const K& k) { return _compare(pair.first, k); });
		if (it != _data.end() && it->first == key)
		{
			_data.erase(it);
			return true;
		}

		return false;
	}

	Iterator begin() noexcept { return _data.begin(); }
	ConstIterator begin() const noexcept { return _data.begin(); }

	Iterator end() noexcept { return _data.end(); }
	ConstIterator end() const noexcept { return _data.end(); }

	auto size() const noexcept { return _data.size(); }
	bool empty() const noexcept { return _data.empty(); }

	void clear() noexcept { _data.clear(); }
	void reserve(std::size_t capacity) { _data.reserve(capacity); }
	auto capacity() const noexcept { return _data.capacity(); }

private:

	ContainerType	_data;
	Compare			_compare;
};	//	

}	//	namespce CxxNote

#endif	//	!__CXXNOTE_FLATMAP_H__
