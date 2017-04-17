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

//����
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

	//��ʼ��������
	if (!InitMutex())
	{
		return FALSE;
	}

	//��ʼ��Redis����
	for (int i = 0; i < m_wRedisConnNum; i ++)
	{
		if (!m_pRedisConnList[i].Open(aszIP,awPort))
		{
			return FALSE;
		}
	}

	//����ά���߳�
	CBaseThread loThread;
	loThread.BeginThread(MaintenanceThread,this);

	return TRUE;
}
//�ر�
void CRedisManager::Close()
{
	m_bStop = TRUE;
	DWORD	ldw_object;
	for (int i = 0; i < m_wRedisConnNum; i ++)
	{
		ldw_object = WaitForSingleObject(m_pMutex[i], 300000); //5����

		if (ldw_object != WAIT_OBJECT_0 )
		{    
			continue;
		}

		// ���������
		CloseHandle(m_pMutex[i]);

		// ���Redis����
		
	}
	if (m_pMutex != NULL)
	{
		delete []m_pMutex;
		m_pMutex = NULL;
	}
}
//ִ��һ��Redisָ��
BOOL CRedisManager::ExecuteRedisCommand(redisReply** apRedisReply,WORD &awRedisIndex,const char *aszformat, ...)
{
	WORD	lwRedisPos = 0;
	//��ȡRedis����Pos(����)
	if (!GetInstancePos(lwRedisPos))
	{
		return FALSE;
	}

	va_list lap;
	va_start(lap,aszformat);

	//ִ��Redisָ��
	CRedisInstance &loRedisInstance = m_pRedisConnList[lwRedisPos];
	*apRedisReply = loRedisInstance.ExecuteRedisCommand(aszformat,lap);

	va_end(lap);

	return TRUE;
}

//�ͷ�RedisӦ��
void CRedisManager::FreeReply(redisReply* apreply,WORD awRedisIndex)
{
	if(NULL != apreply)
	{
		freeReplyObject(apreply);
		apreply = NULL;
	}
	FreeInstancePos(awRedisIndex);
}

//��ʼ����
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
		// ��ʼ��������
		m_pMutex[i] = CreateMutex(NULL, FALSE, NULL);
		if (m_pMutex[i] == NULL)
		{
			return FALSE;
		}
	}
	return TRUE;
}
//Ϊ��������ִ�еķ���һ��Redis����ʵ��Pos
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
//�ͷű�������ִ�л�ȡ��Redis����ʵ��Pos
BOOL CRedisManager::FreeInstancePos(WORD awRedisPos)
{
	if (awRedisPos < m_wRedisConnNum && awRedisPos>=0)
	{
		ReleaseMutex(m_pMutex[awRedisPos]);
	}
	return TRUE;
}

//ά���߳�
unsigned int CRedisManager::MaintenanceThread(void *apParam)
{
	CRedisManager* lpThis = static_cast<CRedisManager*>(apParam);
	if (lpThis != NULL)
	{
		lpThis->KeepConntectionThread();
	}
	return 0;
}
//ά���߳�ִ�к���
unsigned int CRedisManager::KeepConntectionThread()
{
	m_bStop = FALSE;
	while (!m_bStop)
	{
		Sleep(1000);
		for (int i = 0; i < m_wRedisConnNum; i++)
		{
			DWORD	ldw_object;
			ldw_object = WaitForSingleObject(m_pMutex[i], 2000); //2��

			if (ldw_object != WAIT_OBJECT_0 )
			{    
				continue;
			}

			// ���Redis����
			CRedisInstance &loRedisInstance = m_pRedisConnList[i];
			ENUM_REDIS_CONN_STATUS lConnStatus = loRedisInstance.GetConnStatus();
			switch(lConnStatus)
			{
			case REDIS_CONN_INIT:
			case REDIS_CONN_FAIL:
				loRedisInstance.ReConnect(m_szIpAdd,m_wPort);
				break;
			case REDIS_CONN_RUNNING:
				//������ӳ�ʱ������������
				if ((loRedisInstance.GetLastTime() - CBaseThread::GetSystemTime()) >= 90000 )  //1.5��
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

			// ����
			ReleaseMutex(m_pMutex[i]);

		}
	}
	return 0;
}