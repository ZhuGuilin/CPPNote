#include <iostream>
#include "Observer.h"


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