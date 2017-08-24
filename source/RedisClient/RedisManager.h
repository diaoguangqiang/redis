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
	//启动
	BOOL Open(const char* aszIP, WORD awPort, WORD awConnNum);
	//关闭
	void Close();

	//执行一条Redis指令
	BOOL ExecuteRedisCommand(redisReply** apRedisReply,WORD &awRedisIndex,const char *aszformat, ...);
	//释放Redis应答
	void FreeReply(redisReply* apreply,WORD awRedisIndex);

private:
	//初始化锁
	BOOL InitMutex(void);
	//为本次命令执行的分配一个Redis连接实例Pos
	BOOL GetInstancePos(WORD & awRedisPos);
	//释放本次命令执行获取的Redis连接实例Pos
	BOOL FreeInstancePos(WORD awRedisPos);
private:
	//维护线程
	static unsigned int MaintenanceThread(void *apParam);
	//维护线程执行函数
	unsigned int KeepConntectionThread();
private:
	char	m_szIpAdd[DEF_IP_ADDR_LEN+1];				//IP地址
	WORD	m_wPort;									//端口

	WORD	m_wRedisConnNum;							//Redis连接数
	CRedisInstance	*m_pRedisConnList;					//Redis连接表
	HANDLE	*m_pMutex;									//锁

	BOOL	m_bStop;									//停止标记

};
