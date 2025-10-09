#pragma once

#include <vector>


class Observer
{
public:

	Observer();
	virtual ~Observer();

	virtual void Test() = 0;
};

class ObsMgr
{
public:

	ObsMgr(ObsMgr const&) = delete;
	ObsMgr(ObsMgr&&) = delete;
	ObsMgr& operator=(ObsMgr const&) = delete;
	ObsMgr& operator=(ObsMgr&&) = delete;

	inline static ObsMgr& instance() noexcept
	{
		static ObsMgr sObsMgr;
		return sObsMgr;
	}

	void Run() const;
	void Attach(Observer* obs);
	void Detach(Observer* obs);

private:

	ObsMgr() = default;		//	私有构造函数，阻止被 new

	std::vector<Observer*> _observers;
};