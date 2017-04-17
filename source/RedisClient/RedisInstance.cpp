#include "StdAfx.h"
#include ".\redisinstance.h"

CRedisInstance::CRedisInstance(void)
{
	m_pRedisContext = NULL;
	m_enumConn = REDIS_CONN_INIT;
	m_i64LastTime = 0;
}

CRedisInstance::~CRedisInstance(void)
{
	Close();
}

//����Redis
BOOL CRedisInstance::Open(const char* aszIP, WORD awPort)
{
	ASSERT(m_pRedisContext==NULL);

#ifdef _WIN32  
	WSADATA lt_wsa; 
	WORD lwVers = MAKEWORD(2, 2); // Set the version number to 2.2
	int iError = WSAStartup(lwVers, &lt_wsa); 

	if(iError != NO_ERROR || LOBYTE(lt_wsa.wVersion) != 2 || HIBYTE(lt_wsa.wVersion) != 2 ) {
		return FALSE;
	}
#endif  

	struct timeval tv;				//1.5����
	tv.tv_sec = 1;
	tv.tv_usec = 500000;
	m_pRedisContext = redisConnectWithTimeout(aszIP, awPort, tv);
	if (m_pRedisContext ==NULL || 0 != m_pRedisContext->err)
	{
		return FALSE;
	}
	m_enumConn = REDIS_CONN_RUNNING;
	m_i64LastTime = CBaseThread::GetSystemTime();

	return TRUE;
}
//�ر�����
void CRedisInstance::Close(void)
{
	if (m_pRedisContext != NULL)
	{
		redisFree(m_pRedisContext);
		m_pRedisContext = NULL;
	}
}
//ִ��һ��Redisָ��
redisReply* CRedisInstance::ExecuteRedisCommand(const char *aszformat,va_list lpVa)
{
	redisReply *lpReply = NULL;
	if (m_pRedisContext == NULL)
	{
		return NULL;
	}
	if (m_enumConn != REDIS_CONN_RUNNING)
	{
		return NULL;
	}

	lpReply = (redisReply*)redisvCommand(m_pRedisContext,aszformat,lpVa);

	m_i64LastTime = CBaseThread::GetSystemTime();
	if (lpReply == NULL)
	{
		switch(m_pRedisContext->err)
		{
		case REDIS_ERR_PROTOCOL:
		case REDIS_ERR_IO:
		case REDIS_ERR_EOF:
		case REDIS_ERR_OTHER:
			//��������ʧ��
			m_enumConn = REDIS_CONN_FAIL;
			//�ر�����
			Close();
			break;
		default:
			break;
		}
	}
	return lpReply;
}

//����Redis
BOOL CRedisInstance::ReConnect(const char* aszIP, WORD awPort)
{
	//�رյ�ǰ����
	Close();
	return Open(aszIP,awPort);
}

ENUM_REDIS_CONN_STATUS CRedisInstance::GetConnStatus(void)
{
	return m_enumConn;
}
INT64 CRedisInstance::GetLastTime(void)
{
	return m_i64LastTime;
}