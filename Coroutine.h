#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <coroutine>

#include "Observer.h"

class STL_Coroutine : public Observer
{
public:

	

	void Test() override
	{
		std::cout << " ===== STL_Coroutine Bgein =====" << std::endl;


		std::cout << " ===== STL_Coroutine End =====" << std::endl;
	}

};

/*
Э����һ�ֿ���ͣ�ͻָ��ĺ���:

	������ִ�й�������ͣ�����浱ǰ״̬
	�Ժ����ͣ��ָ�ִ��
	�������оֲ�����״̬
	����Ҫ����ϵͳ�߳��л�

����			��ͳ����				Э��
ִ������	|	һ��ִ�����			�ɷֶ��ִ��
״̬����	|	�ޣ��ֲ��������٣�	����ȫ���ֲ�״̬
���ÿ���	|	ջ֡����/����			��ʼ�����󣬻ָ�����С
����ģ��	|	�������߳�			���߳̿ɹ�����ǧЭ��

Э�̺������:
	Э�̾����std::coroutine_handle - ����Э����������
	��ŵ����promise_type - ����Э����Ϊ
	Э��֡�����������ɵ��������ݽṹ���洢Э��״̬

promise_type�����������Ƕ������
	get_return_object()������Э�̵ķ���ֵ����
	initial_suspend()����ʼ�������
	final_suspend()�����չ������
	return_void()������co_return
	unhandled_exception()���쳣����

�������ͣ�
	std::suspend_always�����ǹ���
	std::suspend_never���Ӳ�����

Э���������ڣ�
	����������Э�̺���ʱ
	����co_await��co_yield
	�ָ���handle.resume()
	���٣�handle.destroy()

Э������ؼ���
	1. co_await - �첽�ȴ�
	2. co_yield - ����ֵ
	3. co_return - ����ֵ
*/