#pragma once

#include <iostream>
#include <memory>
#include <functional>
#include <future>

#include "Observer.h"

class STL_Future : public Observer
{
public:

	void Test() override
	{
		std::cout << " ===== STL_Future Bgein =====" << std::endl;


		std::cout << " ===== STL_Future End =====" << std::endl;
	}

};

/*
std::future ���ִ�C++�첽��̵Ļ�ʯ��

���ļ�ֵ����������ִ�кͽ����ȡ
�ؼ����ԣ������ȴ�������������쳣����

���ó�����
	�̳߳������ύ
	�����㷨ʵ��
	�첽I/O����
	�¼������ܹ�

��ʵ��ʹ���У�
	����ʹ�� std::async �������첽����
	���̳߳���ʹ�� std::packaged_task + future
	���̵߳ȴ�ʹ�� std::shared_future
	�������������������Ч��״̬
	���C++20Э��ʵ�ָ����ŵ��첽����
*/