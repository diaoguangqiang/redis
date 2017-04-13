/************************************************************************/
/* redis: ������                                                                     */
/************************************************************************/

#include <stdio.h>

#ifdef WIN32
	#include <time.h>
	#include "Win32_Interop/Win32_Portability.h"
	#include "Win32_Interop/win32_types.h"
	#include "Win32_Interop/Win32_Time.h"
	#include "Win32_Interop/win32fixes.h"
	#include "Win32_Interop/Win32_Signal_Process.h"
	#include "Win32_Interop/Win32_ANSI.h"

	#include <windows.h>

	#include "async.h"
	#include "adapters/ae.h"
#else
	#include <sys/time.h>
	#include <signal.h>
	#include <unistd.h>
#endif // WIN32


#ifdef WIN32
	HANDLE  m_event_thread_;
	HANDLE	m_event_sem_;
#else
	pthread_t		m_event_thread_;// �¼��߳�ID  	
	sem_t			m_event_sem_; // �¼��̵߳��ź���  
#endif // WIN32	

//����״̬
int m_connected_ = -1;
//
aeEventLoop *m_ploop_ = NULL;
// hiredis�첽����  
redisAsyncContext *m_predis_context_ = NULL;

/**/
int redis_init() {

	// ����ae����
	//�����¼�ѭ����setsizeΪ����¼��ĵĸ���������epoll��˵Ҳ��epoll_event�ĸ���
	m_ploop_ = aeCreateEventLoop(1024*10);

	if (NULL == m_ploop_)
	{
		printf("Create redis event failed.\n");
		return -1;
	}

#ifdef WIN32
	//
	m_event_sem_ = CreateSemaphoreA(
		NULL, // security attributes  
		0/*initvalue*/, // initial count  
		0x7fffffff, // maximum value  
		NULL); // name  
#else
	memset(&m_event_sem_, 0, sizeof(m_event_sem_));
	//��ʼ���߳��ź���
	int ret = sem_init(&m_event_sem_, 0, 0);
	if (ret != 0)
	{
		printf("Init sem failed.\n");
		return -1;
	}
#endif // WIN32	

	return 0;
}

/**/
void redis_uninit()
{
	m_ploop_ = NULL;

#ifdef WIN32
	CloseHandle(m_event_sem_);
#else  
	sem_destroy(&m_event_sem_);
#endif  

	return ;
}


void event_thread() {

	WaitForSingleObject(m_event_sem_, INFINITE);
	
	//�����¼�����ѭ��  
	aeMain(m_ploop_);

	return ;
}

/**/
void connect_callback(const redisAsyncContext *_redis_context, int _status)
{
	if (_status != REDIS_OK)
	{
		m_connected_ = -1;
		printf("connect error: %s\n", _redis_context->errstr);		
	}
	else
	{
		m_connected_ = 0;
		printf("connect status:%d\n", _status);
	}

	return;
}

/**/
void disconnect_callback(const redisAsyncContext *_redis_context, int _status)
{
	if (_status != REDIS_OK)
	{
		m_connected_ = -1;
		printf("disconnect success! %s\n", _redis_context->errstr);
	}

	return;
}

/**/
int redis_connect()
{
	// �첽���ӵ�redis�������ϣ�ʹ��6379�˿�
	m_predis_context_ = redisAsyncConnect("192.168.56.101", 6379);

	if (NULL == m_predis_context_)
	{
		printf("Connect redis failed.\n");
		return -1;
	}

	if (m_predis_context_->err)
	{
		printf("Connect redis error: %d, %s\n", m_predis_context_->err, m_predis_context_->errstr);    // ���������Ϣ  
		return -1;
	}

	// ���¼��󶨵�redis context�ϣ�ʹredis�Ļص����¼�����
	redisAeAttach(m_ploop_, m_predis_context_);

	//int ret = pthread_create(&m_event_thread_, NULL, event_thread, NULL);
	uintptr_t ret = _beginthreadex(NULL, 0, event_thread, NULL, 0, NULL);
	if (ret == 0)
	{
		CloseHandle(m_event_thread_);//������ص� �ᷢ���ڴ�������й©
	}

	// �������ӻص������첽�������Ӻ󣬷������������������������ã�֪ͨ���������ӵ�״̬  
	redisAsyncSetConnectCallback(m_predis_context_, &connect_callback);

	// ���öϿ����ӻص������������Ͽ����Ӻ�֪ͨ���������ӶϿ��������߿��������������ʵ������  
	redisAsyncSetDisconnectCallback(m_predis_context_, &disconnect_callback);

	// �����¼��߳�  
	ReleaseSemaphore(m_event_sem_, 1, NULL);

	return 0;
}

/**/
void command_callback(redisAsyncContext *redis_context, void *reply)
{
	if (NULL == reply)
	{
		printf("no reply! line[%d]\n", __LINE__);
		return;
	}

	//redisReply *redis_reply = reinterpret_cast<redisReply *>(reply);
	redisReply *redis_reply = (redisReply*)reply;

	//3, 0,������������int����ȡֵ
	printf("publish success : %d %d %d\n", redis_reply->integer, redis_reply->type, redis_reply->elements );

	return;
}

/**/
int redis_publish(const char* channel_name, const char* message)
{
	//������Ϣ
	int ret = redisAsyncCommand(m_predis_context_, &command_callback, NULL, "PUBLISH %s %s", channel_name, message);

	if (REDIS_ERR == ret)
	{
		printf("publish command failed: %d\n", ret);
		return -1;
	}

	return 0;
}

/**/
int event_wait() {
#ifdef WIN32  
	return WAIT_OBJECT_0 == WaitForSingleObject(m_event_sem_, 0);
#else  
	return 0 == ::sem_trywait(&m_event_sem_);
#endif  
	return 0;
}

/**/
int redis_disconnect() {
	if (m_predis_context_){
		redisAsyncDisconnect(m_predis_context_);
		redisAsyncFree(m_predis_context_);
		m_predis_context_ = NULL;

		return 0;
	}

	return -1;
}

/*------------------------------------------------------------------------------
* Program main()
*--------------------------------------------------------------------------- */
int main(int argc, char **argv) {

	printf("start! line[%d]\n", __LINE__);
	
	redis_init();

	redis_connect();

	char val[128] = { 0 };
	char key[64] = { 0 };

	int index = 0;

	int ret = 0;

	while (1)
	{
		usleep(1000000);

		memset(key, 0, sizeof(key));
		memset(val, 0, sizeof(val));

		snprintf(key, sizeof(key), "key_%d", index);
		snprintf(val, sizeof(val), "value_%d", index);

		if (m_connected_ >= 0)
		{
			ret = redis_publish(key, val);
			printf("> publish %s %s\n", key, val);
		}

		if (index++ >= 100000)
			index = 0;

		if (event_wait())
			break;
	}

	redis_disconnect();
	redis_uninit();

	getchar();

	return 0;
}
