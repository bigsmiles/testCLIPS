/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*               CLIPS Version 6.24  07/01/05          */
/*                                                     */
/*                     MAIN MODULE                     */
/*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Moved UserFunctions and EnvUserFunctions to    */
/*            the new userfunctions.c file.                  */
/*                                                           */
/*************************************************************/

/***************************************************************************/
/*                                                                         */
/* Permission is hereby granted, free of charge, to any person obtaining   */
/* a copy of this software and associated documentation files (the         */
/* "Software"), to deal in the Software without restriction, including     */
/* without limitation the rights to use, copy, modify, merge, publish,     */
/* distribute, and/or sell copies of the Software, and to permit persons   */
/* to whom the Software is furnished to do so.                             */
/*                                                                         */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT   */
/* OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY  */
/* CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES */
/* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN   */
/* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF */
/* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.          */
/*                                                                         */
/***************************************************************************/

//#include <windows.h>

#include <stdio.h>
#include "setup.h"
#include "clips.h"

#include "move.h"
#include "input.h"


int main(int, char *[]);
void UserFunctions(void);
void EnvUserFunctions(void *);

/****************************************/
/* main: Starts execution of the expert */
/*   system development environment.    */
/****************************************/



#if THREAD 

CRITICAL_SECTION g_cs;
//CRITICAL_SECTION g_debug; //解决同步问题?
CRITICAL_SECTION g_csDebug, g_runDebug, g_move,g_fact_join;//g_csDebug1, g_csDebug2,
HANDLE g_debug;
HANDLE g_hSemaphoreBuffer,g_hSemaphoreBufferOfThread1, g_hSemaphoreBufferOfThread2;
extern int totalAddActiveNode,totalGetActiveNode[4];

char print_file_path[3][60] = { "","D:\\VS\\testCLPS\\testCLIPS\\Debug\\result_window0821a.txt", "D:\\VS\\testCLPS\\testCLIPS\\Debug\\result_window0821b.txt" };
FILE *print_file[3];
LARGE_INTEGER search_time1, search_time2;
long long search_time = 0, cost_time[4] = { 0 }, run_time[4] = {0};
long long cur_partialmatch_time[3] = {99999999999,999999999999,999999999999};

int num_of_event;
#endif



int main(
	int argc,
	char *argv[])
{
	void *theEnv;

#if THREAD
	void *betaEnv;
	void *thirdEnv;
	void *fourEnv;

#if SPINCOUNT
	InitializeCriticalSection(&g_cs);
	InitializeCriticalSection(&g_move);
	InitializeCriticalSection(&g_runDebug);
	InitializeCriticalSection(&g_fact_join);
#else if
	
	InitializeCriticalSectionAndSpinCount(&g_cs,0x00000400);
	InitializeCriticalSectionAndSpinCount(&g_move,0x00000400);
	InitializeCriticalSectionAndSpinCount(&g_runDebug, 0x00000400);
	InitializeCriticalSectionAndSpinCount(&g_fact_join, 0x00000400);
#endif

#if !MUTILTHREAD
	g_hSemaphoreBuffer = CreateSemaphore(NULL, 0, 30000000, NULL);
#else
	g_hSemaphoreBufferOfThread1 = CreateSemaphore(NULL, 0, 20000, NULL);
	g_hSemaphoreBufferOfThread2 = CreateSemaphore(NULL, 0, 20000, NULL);
#endif
	//g_debug = CreateSemaphore(NULL, 0, 1, NULL);
	HANDLE hThread,hThread1,hThread2;
#endif

	
	theEnv = CreateEnvironment();
#if THREAD
	betaEnv = CreateEnvironment();
	thirdEnv = CreateEnvironment();
	fourEnv = CreateEnvironment();
#endif
	
	//char rule_file_path[50] = "D:\\VS\\testCLPS\\testCLIPS\\Debug\\CLIPSRule_test.clp";
	//char rule_file_path[50] = "D:\\VS\\testCLPS\\testCLIPS\\Debug\\CLIPSRule_824.clp";
	char rule_file_path[50] = "D:\\VS\\testCLPS\\testCLIPS\\Debug\\CLIPSRule_826.clp";
	//char rule_file_path[50] = "D:\\VS\\testCLPS\\testCLIPS\\Debug\\CLIPSRule_1025.clp";
	//char rule_file_path[50] = "D:\\VS\\testCLPS\\testCLIPS\\Debug\\CLIPSRule_723.clp";

	
	EnvLoad(theEnv, rule_file_path);

#if THREAD || REALMTHREAD

	EnvLoad(betaEnv, rule_file_path);
	EnvLoad(thirdEnv, rule_file_path);
	EnvLoad(fourEnv, rule_file_path);

	struct ThreadNode *env1 = (struct ThreadNode*)malloc(sizeof(struct ThreadNode));
	env1->theEnv = betaEnv; env1->threadTag = 1;
	struct ThreadNode *env2 = (struct ThreadNode*)malloc(sizeof(struct ThreadNode));
	env2->theEnv = thirdEnv; env2->threadTag = 2;
	struct ThreadNode *env3 = (struct ThreadNode*)malloc(sizeof(struct ThreadNode));
	env3->theEnv = fourEnv; env3->threadTag = 3;
	
#endif	
	//第一个参数是event or fact的个数，第二个参数是并行或串行parallel_serial，并行为1，串行为0
	num_of_event = 20000;
	int parallel_serial = 1;
	if (argc > 1) num_of_event = atoi(argv[1]);
	if (argc > 2) parallel_serial = atoi(argv[2]);
	
#if THREAD
	//add by xuchao,start this execute thread
	/**/
	if (parallel_serial == 1)
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env1, 0, NULL);
		SetThreadAffinityMask(hThread, 1 << 1);//线程指定在某个cpu运行
		hThread1 = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env2, 0, NULL);
		SetThreadAffinityMask(hThread1, 1 << 2);//线程指定在某个cpu

		hThread2 = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env3, 0, NULL);
		SetThreadAffinityMask(hThread2, 1 << 3);//线程指定在某个cpu运行
	}
	/**/
#endif

#if AUTOTEST
	
	//char fact_file_path[50] = "D:\\VS\\stdCLIPS\\Debug\\CLIPSFact_test.txt"; 
	char fact_file_path[50] = "D:\\VS\\stdCLIPS\\Debug\\CLIPSFact_105.txt";
	//char fact_file_path[50] = "D:\\VS\\stdCLIPS\\Debug\\CLIPSFact_723.txt";
	
	FILE *pFile = fopen(fact_file_path, "r");
	print_file[1] = fopen(print_file_path[1], "w");
	print_file[0] = fopen(print_file_path[2], "w");
	
	if (pFile == NULL){
		printf("file error!\n");
	}
	
	
	LARGE_INTEGER start,end,finish,freq;
	int cnt = 0;
	
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	printf("system_start_time: 0  %lld\n", start.QuadPart);

	
	long long line_count = 1;
	char tmpBuffer[200];
	while (fgets(tmpBuffer, 100, pFile))
	//while (line_count--)
	{
		

		//printf("%s\n", tmpBuffer); break;
		if (line_count++ > num_of_event)break;
		//if(line_count % 5 == 0)Sleep(1);
		EnvAssertString(theEnv, tmpBuffer);
		
		
	} 
	QueryPerformanceCounter(&end);
	printf("input_time_over %lld ,total: %lld,line_count %lld\n", end.QuadPart,(end.QuadPart - start.QuadPart) / freq.QuadPart,line_count);
	
	if (parallel_serial == 0)
	{

		hThread = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env1, 0, NULL);
		SetThreadAffinityMask(hThread, 1 << 1);//线程指定在某个cpu运行
		hThread1 = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env2, 0, NULL);
		SetThreadAffinityMask(hThread1, 1 << 2);//线程指定在某个cpu运行

		hThread2 = (HANDLE)_beginthreadex(NULL, 0, MoveOnJoinNetworkThread, env3, 0, NULL);
		SetThreadAffinityMask(hThread2, 1 << 3);//线程指定在某个cpu运行
	}
	Sleep(100000);
	//CommandLoop(theEnv);
#if !THREAD 
	CommandLoop(theEnv);
#endif
	QueryPerformanceCounter(&finish);
	
	printf("search_time: %lld %lf %lf\n", search_time, cost_time[0] * 1.0 / freq.QuadPart, run_time[0] * 1.0 / freq.QuadPart);
	
	//CommandLoop(theEnv);
#else 
	CommandLoop(theEnv);
	//CommandLoop(betaEnv);
	//CommandLoop(thirdEnv);
	
#endif 
#if THREAD
	CloseHandle(g_hSemaphoreBufferOfThread1);
	CloseHandle(g_hSemaphoreBufferOfThread2);
	CloseHandle(g_debug);
	//DeleteCriticalSection(&g_cs);
	DeleteCriticalSection(&g_runDebug);
	CloseHandle(hThread);
	
#endif

	/*==================================================================*/
	/* Control does not normally return from the CommandLoop function.  */
	/* However if you are embedding CLIPS, have replaced CommandLoop    */
	/* with your own embedded calls that will return to this point, and */
	/* are running software that helps detect memory leaks, you need to */
	/* add function calls here to deallocate memory still being used by */
	/* CLIPS. If you have a multi-threaded application, no environments */
	/* can be currently executing. If the ALLOW_ENVIRONMENT_GLOBALS     */
	/* flag in setup.h has been set to TRUE (the default value), you    */
	/* call the DeallocateEnvironmentData function which will call      */
	/* DestroyEnvironment for each existing environment and then        */
	/* deallocate the remaining data used to keep track of allocated    */
	/* environments. Otherwise, you must explicitly call                */
	/* DestroyEnvironment for each environment you create.              */
	/*==================================================================*/

	/* DeallocateEnvironmentData(); */
	/* DestroyEnvironment(theEnv); */
	
	return(-1);
}
