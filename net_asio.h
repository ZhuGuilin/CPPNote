#pragma once

#include "Observer.h"


class net_asio : public Observer
{
public:

	net_asio();
	virtual ~net_asio();
	virtual void Test() override;
};
