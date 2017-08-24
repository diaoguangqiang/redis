/************************************************************************/
/* redis: ���Ķ�                                                                    */
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
	m_ploop_ = aeCreateEventLoop(1024 * 10);

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

	return;
}


void event_thread() {

	WaitForSingleObject(m_event_sem_, INFINITE);

	//�����¼�����ѭ��  
	aeMain(m_ploop_);

	return;
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
		printf("connect status:%d line[%d]\n", _status, __LINE__);
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
	m_predis_context_ = redisAsyncConnect("123.206.6.148", 6379);

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
	if ( redis_reply->type == REDIS_REPLY_ARRAY &&
		redis_reply->elements == 3 )
	{
		printf(": subscribe message:%s:%d, %s:%d, %s:%d\n",
			redis_reply->element[0]->str, redis_reply->element[0]->len,
			redis_reply->element[1]->str, redis_reply->element[1]->len,
			redis_reply->element[2]->str, redis_reply->element[2]->len);
	}
	
	return;
}

/**/
int redis_subscribe(const char* channel_name)
{
	printf("subscribe[%s] line[%d]\n", channel_name, __LINE__);

	//������Ϣ
	int ret = redisAsyncCommand(m_predis_context_, &command_callback, NULL, "SUBSCRIBE %s", channel_name);

	if (REDIS_ERR == ret)
	{
		printf("subscribe command failed: %d\n", ret);
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
	if (m_predis_context_) {
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

	char key[64] = { 0 };

	snprintf(key, sizeof(key), "hello");

	while (1)
	{
		if (m_connected_ >= 0)
		{
			int ret = redis_subscribe(key);
			break;
		}
	}
	

	while (1)
	{
		usleep(1000000);

		if (event_wait())
			break;
	}

	redis_disconnect();
	redis_uninit();

	getchar();

	return 0;
}
