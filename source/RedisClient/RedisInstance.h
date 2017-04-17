#pragma once
#include "hiredis.h"
#include "DebugTrace.h"
#include "BaseThread.h"
#include "ConstDeff.h"

enum ENUM_REDIS_CONN_STATUS
{
	REDIS_CONN_INIT = 1,		//Redis连接初始状态
	//REDIS_CONN_IDLE,			//Redis连接空闲
	REDIS_CONN_RUNNING,			//Redis连接运行中
	REDIS_CONN_FAIL,			//Redis连接断开
};
class CRedisInstance
{
public:
	CRedisInstance(void);
	~CRedisInstance(void);
public:
	//连接Redis
	BOOL Open(const char* aszIP, WORD awPort);
	//关闭连接
	void Close(void);
	//执行一条Redis指令
	redisReply* ExecuteRedisCommand(const char *aszformat,va_list lpVa=NULL);

	//重连Redis
	BOOL ReConnect(const char* aszIP, WORD awPort);

public:
	ENUM_REDIS_CONN_STATUS GetConnStatus(void);
	INT64 GetLastTime(void);
	
private:
	redisContext				*m_pRedisContext;		//Redis连接上下文
	ENUM_REDIS_CONN_STATUS		m_enumConn;				//Redis连接状态
	INT64						m_i64LastTime;			//该连接上次执行命令时间
};
