#include "StdAfx.h"
#include ".\redismanager.h"

CRedisManager::CRedisManager(void)
{
	memset(m_szIpAdd,0,sizeof(m_szIpAdd));
	m_wPort = 0;
	m_wRedisConnNum = 0;
	m_pRedisConnList = NULL;
	m_bStop = TRUE;
	m_pMutex = NULL;
}

CRedisManager::~CRedisManager(void)
{
	Close();
}

//启动
BOOL CRedisManager::Open(const char* aszIP, WORD awPort, WORD awConnNum)
{
	ASSERT((m_wRedisConnNum==0)&&(m_pRedisConnList==NULL));
	strcpy(m_szIpAdd,aszIP);
	m_wPort = awPort;
	m_wRedisConnNum = awConnNum;
	m_pRedisConnList = new CRedisInstance[m_wRedisConnNum];
	if (m_pRedisConnList==NULL)
	{
		return FALSE;
	}

	//初始化锁对象
	if (!InitMutex())
	{
		return FALSE;
	}

	//初始化Redis连接
	for (int i = 0; i < m_wRedisConnNum; i ++)
	{
		if (!m_pRedisConnList[i].Open(aszIP,awPort))
		{
			return FALSE;
		}
	}

	//启动维护线程
	CBaseThread loThread;
	loThread.BeginThread(MaintenanceThread,this);

	return TRUE;
}
//关闭
void CRedisManager::Close()
{
	m_bStop = TRUE;
	DWORD	ldw_object;
	for (int i = 0; i < m_wRedisConnNum; i ++)
	{
		ldw_object = WaitForSingleObject(m_pMutex[i], 300000); //5分钟

		if (ldw_object != WAIT_OBJECT_0 )
		{    
			continue;
		}

		// 清除互斥量
		CloseHandle(m_pMutex[i]);

		// 清除Redis连接
		
	}
	if (m_pMutex != NULL)
	{
		delete []m_pMutex;
		m_pMutex = NULL;
	}
}
//执行一条Redis指令
BOOL CRedisManager::ExecuteRedisCommand(redisReply** apRedisReply,WORD &awRedisIndex,const char *aszformat, ...)
{
	WORD	lwRedisPos = 0;
	//获取Redis连接Pos(加锁)
	if (!GetInstancePos(lwRedisPos))
	{
		return FALSE;
	}

	va_list lap;
	va_start(lap,aszformat);

	//执行Redis指令
	CRedisInstance &loRedisInstance = m_pRedisConnList[lwRedisPos];
	*apRedisReply = loRedisInstance.ExecuteRedisCommand(aszformat,lap);

	va_end(lap);

	return TRUE;
}

//释放Redis应答
void CRedisManager::FreeReply(redisReply* apreply,WORD awRedisIndex)
{
	if(NULL != apreply)
	{
		freeReplyObject(apreply);
		apreply = NULL;
	}
	FreeInstancePos(awRedisIndex);
}

//初始化锁
BOOL CRedisManager::InitMutex(void)
{
	ASSERT(m_pMutex == NULL);
	m_pMutex = new HANDLE[m_wRedisConnNum];
	if (m_pMutex == NULL)
	{
		return FALSE;
	}
	for (int i =0 ; i < m_wRedisConnNum; i++)
	{
		// 初始化互斥量
		m_pMutex[i] = CreateMutex(NULL, FALSE, NULL);
		if (m_pMutex[i] == NULL)
		{
			return FALSE;
		}
	}
	return TRUE;
}
//为本次命令执行的分配一个Redis连接实例Pos
BOOL CRedisManager::GetInstancePos(WORD & awRedisPos)
{
	DWORD ldw_object;
	ldw_object = WaitForMultipleObjects(m_wRedisConnNum, m_pMutex, FALSE, INFINITE);

	if (ldw_object < WAIT_OBJECT_0 || ldw_object >= WAIT_ABANDONED_0)
	{
		return FALSE;
	}

	awRedisPos = (unsigned short)(ldw_object - WAIT_OBJECT_0);

	ASSERT(awRedisPos < m_wRedisConnNum);
	ASSERT(awRedisPos >= 0);
	if((awRedisPos >= m_wRedisConnNum) ||(awRedisPos < 0))
	{
		return FALSE;
	}
	return TRUE;
}
//释放本次命令执行获取的Redis连接实例Pos
BOOL CRedisManager::FreeInstancePos(WORD awRedisPos)
{
	if (awRedisPos < m_wRedisConnNum && awRedisPos>=0)
	{
		ReleaseMutex(m_pMutex[awRedisPos]);
	}
	return TRUE;
}

//维护线程
unsigned int CRedisManager::MaintenanceThread(void *apParam)
{
	CRedisManager* lpThis = static_cast<CRedisManager*>(apParam);
	if (lpThis != NULL)
	{
		lpThis->KeepConntectionThread();
	}
	return 0;
}
//维护线程执行函数
unsigned int CRedisManager::KeepConntectionThread()
{
	m_bStop = FALSE;
	while (!m_bStop)
	{
		Sleep(1000);
		for (int i = 0; i < m_wRedisConnNum; i++)
		{
			DWORD	ldw_object;
			ldw_object = WaitForSingleObject(m_pMutex[i], 2000); //2秒

			if (ldw_object != WAIT_OBJECT_0 )
			{    
				continue;
			}

			// 检查Redis连接
			CRedisInstance &loRedisInstance = m_pRedisConnList[i];
			ENUM_REDIS_CONN_STATUS lConnStatus = loRedisInstance.GetConnStatus();
			switch(lConnStatus)
			{
			case REDIS_CONN_INIT:
			case REDIS_CONN_FAIL:
				loRedisInstance.ReConnect(m_szIpAdd,m_wPort);
				break;
			case REDIS_CONN_RUNNING:
				//如果连接超时，发心跳报文
				if ((loRedisInstance.GetLastTime() - CBaseThread::GetSystemTime()) >= 90000 )  //1.5分
				{
					redisReply *lpReply = (redisReply*)loRedisInstance.ExecuteRedisCommand("PING");
					if (lpReply != NULL)
					{
						freeReplyObject(lpReply);
						lpReply = NULL;
					}
				}
				break;
			default:
				break;
			}

			// 放锁
			ReleaseMutex(m_pMutex[i]);

		}
	}
	return 0;
}