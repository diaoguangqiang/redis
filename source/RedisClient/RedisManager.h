#pragma once

#include <map>
#include "RedisInstance.h"

typedef int thread_id;

class CRedisManager
{
public:
	CRedisManager(void);
	~CRedisManager(void);
public:
	//����
	BOOL Open(const char* aszIP, WORD awPort, WORD awConnNum);
	//�ر�
	void Close();

	//ִ��һ��Redisָ��
	BOOL ExecuteRedisCommand(redisReply** apRedisReply,WORD &awRedisIndex,const char *aszformat, ...);
	//�ͷ�RedisӦ��
	void FreeReply(redisReply* apreply,WORD awRedisIndex);

private:
	//��ʼ����
	BOOL InitMutex(void);
	//Ϊ��������ִ�еķ���һ��Redis����ʵ��Pos
	BOOL GetInstancePos(WORD & awRedisPos);
	//�ͷű�������ִ�л�ȡ��Redis����ʵ��Pos
	BOOL FreeInstancePos(WORD awRedisPos);
private:
	//ά���߳�
	static unsigned int MaintenanceThread(void *apParam);
	//ά���߳�ִ�к���
	unsigned int KeepConntectionThread();
private:
	char	m_szIpAdd[DEF_IP_ADDR_LEN+1];				//IP��ַ
	WORD	m_wPort;									//�˿�

	WORD	m_wRedisConnNum;							//Redis������
	CRedisInstance	*m_pRedisConnList;					//Redis���ӱ�
	HANDLE	*m_pMutex;									//��

	BOOL	m_bStop;									//ֹͣ���

};
