#include <iostream>
#include "Observer.h"

// 通过继承Observer类，自动注册到ObsMgr单例中 方便统一管理和调用测试
Observer::Observer()
{
	ObsMgr::instance().Attach(this);
}

Observer::~Observer()
{
	ObsMgr::instance().Detach(this);
}

void ObsMgr::Run() const
{
	for (auto& obs : _observers)
	{
		printf("\n");
		obs->Test();
		printf("\n");
	}
}

void ObsMgr::Attach(Observer* obs)
{
	_observers.push_back(obs);
}

void ObsMgr::Detach(Observer* obs)
{
	_observers.erase(std::find(_observers.begin(), _observers.end(), obs));
}