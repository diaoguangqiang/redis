// RedisClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "RedisManager.h"

unsigned __stdcall ThreadAllocFunc(void *aoParam)
{

	CRedisManager *lpRedisMgr = (CRedisManager *)(aoParam);
	while(1)
	{
		redisReply * lpReply = NULL;
		WORD lwIndex = 0;
		int j = 0;

		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"PING"))
		{
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		/* Set a key */
		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"SET %s %s", "foo", "hello world"))
		{
			printf("SET: %s\n", lpReply->str);
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		/* Set a key using binary safe API */
		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"SET %b %b", "bar", 3, "hello", 5))
		{
			printf("SET (binary API): %s\n", lpReply->str);
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		/* Try a GET and two INCR */
		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"GET foo"))
		{
			printf("GET foo: %s\n", lpReply->str);
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"INCR counter"))
		{
			printf("INCR counter: %lld\n", lpReply->integer);
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}
		/* again ... */
		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"INCR counter"))
		{
			printf("INCR counter: %lld\n", lpReply->integer);
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		/* Create a list of numbers, from 0 to 9 */
		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"DEL mylist"))
		{
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}

		for (j = 0; j < 10; j++) {
			char buf[64];

			_snprintf(buf,64,"%d",j);

			if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"LPUSH mylist element-%s", buf))
			{
				lpRedisMgr->FreeReply(lpReply,lwIndex);
			}
		}

		/* Let's check what we have inside the list */

		if(lpRedisMgr->ExecuteRedisCommand(&lpReply,lwIndex,"LRANGE mylist 0 -1"))
		{
			if (lpReply->type == REDIS_REPLY_ARRAY) {
				for (j = 0; j < lpReply->elements; j++) {
					printf("%u) %s\n", j, lpReply->element[j]->str);
				}
			}
			lpRedisMgr->FreeReply(lpReply,lwIndex);
		}
		Sleep(200);
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	CRedisManager loRedisMgr;
	loRedisMgr.Open((char*)"192.168.0.55", 6379,10);

	HANDLE hThread;
	unsigned threadID;

	//int i = 0;
	//while(i < 25)
	//{
	//	threadID = i;
	//	// Create the second thread.
	//	hThread = (HANDLE)_beginthreadex( NULL, 0, &ThreadAllocFunc, &loRedisMgr, 0, &threadID );
	//	i++;
	//}

	while(1)
	{
		Sleep(1000);
	}

	return 0;
}

