/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*                                                     */
/*                                                     */
/*                      add by xuchao                  */
/*******************************************************/
#define _MOVE_SOURCE_

#include <stdio.h>
#define _STDIO_INCLUDED_
#include <string.h>

#include "setup.h"

//#include "drive.h"
//#include <windows.h>
#include <process.h>

#include "move.h"
#include "drive.h"
#include "reteutil.h"
#include "factmngr.h"

//add by xuchao
#include "envrnmnt.h"

#define ARRAY 0
#define SALIENCE 0
#define SCHEDULE 0
#if THREAD

struct JoinNodeList *joinNodeListHead;

#if ARRAY
struct joinNode **ArrayOfJoinNode = NULL;
int len = 0;
#endif 

struct JoinNodeList *joinNodeListTail;

extern struct activeJoinNode *activeNodeHead;
extern struct activeJoinNode *activeNodeTail;
extern CRITICAL_SECTION g_cs, g_move;
extern CRITICAL_SECTION g_fact_join;
extern HANDLE  g_hSemaphoreBuffer, g_hSemaphoreBufferOfThread1, g_hSemaphoreBufferOfThread2;

extern LARGE_INTEGER search_time1, search_time2;
extern long long search_time,cost_time[4],run_time[4];
extern long long cur_partialmatch_time[3];
long long totalFromAlpha = 0;

struct activeJoinNode* GetBestOneActiveNode(void *theEnv,int id);
void AddList(struct JoinNodeList*);

int EstimateJoinNodeToEndTime(struct joinNode *curJoinNode);
int totalGetActiveNode[4] = { 0 }, totalAddActiveNode = 0;
globle void MoveOnJoinNetwork(void*theEnv)
{
	struct activeJoinNode *currentActiveNode;
	struct joinNode *currentJoinNode;
	struct partialMatch *currentPartialMatch;
	struct partialMatch *lhsBinds;
	struct partialMatch *rhsBinds;
	unsigned long hashValue;
	char enterDirection;
	struct fact *theFact;
	struct multifieldMarker *theMarks;
	struct patternNodeHeader *theHeader;
	unsigned long hashOffset;

	while ((currentActiveNode = GetBestOneActiveNode(theEnv,0)) != NULL)
	{
		currentJoinNode = currentActiveNode->currentJoinNode;
		currentPartialMatch = currentActiveNode->currentPartialMatch;
		lhsBinds = currentActiveNode->lhsBinds;
		rhsBinds = currentActiveNode->rhsBinds;
		hashValue = currentActiveNode->hashValue;
		enterDirection = currentActiveNode->curPMOnWhichSide;
		theFact =(struct fact *) currentActiveNode->theEntity;
		theMarks = currentActiveNode->markers;
		theHeader = currentActiveNode->theHeader;

		

		if (currentJoinNode->firstJoin)
		{
			// I have modified the CreateAlpahMatch()!! It should named be CreateAlphaMatch if not have else return the partialMatch
			currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			currentPartialMatch->owner = theHeader;

			((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			EmptyDrive(theEnv, currentJoinNode, currentPartialMatch);
		}
		else if (currentActiveNode->curPMOnWhichSide == LHS)
		{
			UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			NetworkAssertLeft(theEnv, currentPartialMatch, currentJoinNode,0);
		}
		else if (currentActiveNode->curPMOnWhichSide == RHS)
		{
			//UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			currentPartialMatch->owner = theHeader;

			((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			NetworkAssertRight(theEnv, currentPartialMatch, currentJoinNode,0);
		}
		else
		{
			//error
		}

	}
	return;
}

//min Time from curNode to endNode
globle int EstimateJoinNodeToEndTime(struct joinNode *curJoinNode){

	
	struct expr *firstTest = curJoinNode->networkTest;
	struct expr *p = firstTest;
	int numberOfTest = 0;
	struct joinLink * CurJoinLink = NULL;
	if (curJoinNode->ruleToActivate != NULL)return 0;

	while (p != NULL){
		numberOfTest += 1;
		p = p->nextArg;
	}
	int minTime = 999999999;
	for (CurJoinLink = curJoinNode->nextLinks; CurJoinLink != NULL; CurJoinLink = CurJoinLink->next){
		int time = EstimateJoinNodeToEndTime(CurJoinLink->join);
		if (time < minTime){
			time = minTime;
		}
	}
	//minTime = 0;
	return numberOfTest + minTime;
}
globle struct activeJoinNode* GetBestOneActiveNode(void *theEnv, int threadID)
{
	struct activeJoinNode *rtnNode = NULL;
	struct activeJoinNode *pNode = NULL;
	struct JoinNodeList *oneListNode = NULL;
	struct JoinNodeList *curListNode = NULL;
	/**/
	LARGE_INTEGER large_time_start;
	QueryPerformanceCounter(&large_time_start);
	
	long long start = (long long)large_time_start.QuadPart;
	/**/
#if THREAD

#if !MUTILTHREAD || REALMTHREAD
	//WaitForSingleObject(g_hSemaphoreBuffer, INFINITE); // xuchao
#else if
	if(rtnNode == NULL){
		if (threadID == 0){
			WaitForSingleObject(g_hSemaphoreBufferOfThread1, INFINITE);
		}
		else{
			WaitForSingleObject(g_hSemaphoreBufferOfThread2, INFINITE);
		}
	}
#endif
	EnterCriticalSection(&g_cs);
#endif
#if SPEEDUP

#if !ARRAY
	if (joinNodeListHead->next != NULL)//return NULL;
		oneListNode = joinNodeListHead->next;
	//long long MinTime = 0x3fffffffffffffff;
	//long long flag = 100000;
	struct joinNode *tmp = NULL;
	//int depth = -1;
	//long long cnt = 0;
#else if
	struct joinNode *tmp = NULL;

	if (ArrayOfJoinNode == NULL && joinNodeListHead->next != NULL){
		struct JoinNodeList *p = joinNodeListHead->next;
		int vsize = 0;
		while (p != NULL){
			vsize += 1;
			p = p->next;
		}

		ArrayOfJoinNode = (struct joinNode*) malloc(sizeof(struct joinNode*) * vsize);
		len = vsize;
		memset(ArrayOfJoinNode, 0, vsize*sizeof(struct joinNode*));
		p = joinNodeListHead->next;
		int i = 0;
		while (p != NULL){
			ArrayOfJoinNode[i++] = p->join;
			//printf("%d %d\n", ArrayOfJoinNode[i], p->join);
			p = p->next;
		}
	}
#endif
	//printf("**%d***\n", len);

#if ARRAY
	int cnt = 0;
	while (rtnNode == NULL){
		int index = rand() % len;
		if (cnt++ > 10)break;
		//printf("%d %d %d\n", index, len, index % len);
		struct joinNode *node = ArrayOfJoinNode[index];
		//printf("****node %d %d %d %d******\n", index, ArrayOfJoinNode[index],node->numOfActiveNode,node->threadTag);
		//Sleep(100);
		if (node != NULL && node->numOfActiveNode > 0 && node->threadTag != -1){
			tmp = node;
			rtnNode = node->activeJoinNodeListHead->next;
		}
	}

#else if
		while (oneListNode != NULL){

			//if (oneListNode->join->numOfActiveNode > 0 )
			{
#if MUTILTHREAD
				if ((oneListNode->join->nodeMaxSalience % 2) == threadID)
#endif
#if REALMTHREAD
				if (oneListNode->join->threadTag != -1)
#endif
				{
					tmp = oneListNode->join;
					curListNode = oneListNode;
					tmp->threadTag = -1;
					break;
				}
			}

			oneListNode = oneListNode->next;
		}
		
#if CSECTION
		LeaveCriticalSection(&g_cs);

		if (oneListNode == NULL){
			return NULL;
		}

		EnterCriticalSection(&(tmp->nodeSection));
#endif

#endif // ARRAY

	if (oneListNode != NULL && oneListNode->join != NULL)
	{
		rtnNode = oneListNode->join->activeJoinNodeListHead->next;
		rtnNode->pre->next = rtnNode->next;
		if (rtnNode->next != NULL){
			rtnNode->next->pre = rtnNode->pre;
		}
		else{
			tmp->activeJoinNodeListTail = tmp->activeJoinNodeListHead;
		}
		tmp->numOfActiveNode -= 1;

#if REALMTHREAD
		//tmp->threadTag = -1;
#if DATASTRUCT
		if (tmp->numOfActiveNode == 0){
#if CSECTION
			EnterCriticalSection(&g_cs);
#endif
			if (curListNode->next == NULL){
				struct JoinNodeList* p = curListNode->pre;
				p->next = NULL;
				joinNodeListTail = p;

				curListNode->join = NULL;
				free(curListNode);
				curListNode = NULL;
			}
			else{
				struct JoinNodeList* p = curListNode->pre;
				p->next = curListNode->next;
				curListNode->next->pre = p;

				curListNode->join = NULL;
				free(curListNode);
				curListNode= NULL;
			}
#if CSECTION
			LeaveCriticalSection(&g_cs);
#endif
		}
#endif
#endif
	}

#else
	if (activeNodeHead->next == NULL)return NULL;
	pNode = activeNodeHead->next;
	rtnNode = pNode;
	long long MinTime = 0x3ffffffffffff;
	long long flag = 100000;

	//no any algorithm ,just take the head(first) one
	if (activeNodeHead->next != NULL) rtnNode = activeNodeHead->next;
	
	/*while(pNode != NULL) 
	{
		if (pNode->currentJoinNode->nodeMaxSalience > rtnNode->currentJoinNode->nodeMaxSalience)
		//int curTime = EstimateJoinNodeToEndTime(pNode->currentJoinNode);  //can be optimized
		//long long curTime = (pNode->currentJoinNode->nodeMinSalience * flag) + pNode->timeTag;
		//if (curTime < MinTime)
		{
			rtnNode = pNode;
			//MinTime = curTime;
		}
		pNode = pNode->next;
	}*/
	rtnNode->pre->next = rtnNode->next;
	if(rtnNode->next != NULL)rtnNode->next->pre = rtnNode->pre;
	if (rtnNode == activeNodeTail) activeNodeTail = rtnNode->pre;
	rtnNode->next = NULL;
	rtnNode->pre = NULL;
#endif
#if THREAD
#if !CSECTION
	LeaveCriticalSection(&g_cs);
#else
	LeaveCriticalSection(&(tmp->nodeSection));
#endif
#if REALMTHREAD
	
	if (rtnNode != NULL){
		
		WaitForSingleObject(g_hSemaphoreBuffer, INFINITE);
		//ReleaseSemaphore(g_hSemaphoreBuffer, 1, NULL);
		/**/
		LARGE_INTEGER large_time;
		QueryPerformanceCounter(&large_time);
		
		long long time = (long long)large_time.QuadPart;
		search_time = time;
		cost_time[threadID] += (time - start);
		totalGetActiveNode[threadID] += 1;
		/**/
	}
#endif

#endif
	return rtnNode;
}
globle void AddList(struct JoinNodeList* oneNode){
#if THREAD
	//EnterCriticalSection(&g_cs);
#endif
	struct JoinNodeList* p = joinNodeListHead->next;
	//struct JoinNodeList* p = NULL;
	
#if !SALIENCE
	/*
	while (p != NULL && p->join->nodeMaxSalience > oneNode->join->nodeMaxSalience){
		p = p->next;
	}
	
	while(p != NULL && p->join->nodeMaxSalience == oneNode->join->nodeMaxSalience && p->join->depth >= oneNode->join->depth){
		p = p->next;
	}
	*/
	while (p != NULL && p->join->depth > oneNode->join->depth){
		p = p->next;
	}
	
#else  //直接放在最后
	
	p = NULL; //place it at list tail
#endif
	if (p == NULL){
		joinNodeListTail->next = oneNode;
		oneNode->pre = joinNodeListTail;
		joinNodeListTail = oneNode;
	}
	else{
		struct JoinNodeList* pre = p->pre;
		pre->next = oneNode; oneNode->pre = pre;
		oneNode->next = p; p->pre = oneNode;
	}
#if THREAD
	//LeaveCriticalSection(&g_cs);
#endif
	return;
}
globle void AddNodeFromAlpha(
	void* theEnv,
	struct joinNode* curNode, //listOfJoins
	unsigned long hashValue,
	struct multifieldMarker *theMarks,
	struct fact* theFact,
	struct patternNodeHeader* header
	)
{
	
	/*DATA_OBJECT timeVal;
	long long time = 0;
	EnvDirectGetSlot(theEnv, (void*)theFact, "time", &timeVal);
	if (GetType(timeVal) == INTEGER){
		
		time = DOToLong(timeVal);
		printf("arg1 is integer: %lld\n", time);
	}
	*/
	
//#if THREAD
//	EnterCriticalSection(&g_cs);
//#endif
	//printf("%d %s\n", curNode->depth, theFact->whichDeftemplate->header.name->contents);
	struct activeJoinNode* oneNode = (struct activeJoinNode*) malloc(sizeof(struct activeJoinNode));
	oneNode->currentJoinNode = curNode;
	//theMatch = NULL;
	oneNode->currentPartialMatch = NULL; //null
	oneNode->curPMOnWhichSide = RHS;
	oneNode->markers = theMarks;
	oneNode->theEntity = theFact;
	oneNode->theHeader = header;// (struct patternNodeHeader *)&thePattern->header;
	oneNode->hashOffset = hashValue;
	oneNode->hashValue = hashValue;
	oneNode->next = NULL;
	int flag = 0;
	struct JoinNodeList *oneListNode = NULL;
	//oneNode->timeTag = time;
#if THREAD
#if !CSECTION
	EnterCriticalSection(&g_cs);
#else
	EnterCriticalSection(&(curNode->nodeSection));
	
	cur_partialmatch_time[0] = theFact->timestamp;
#endif
#endif
#if SPEEDUP
	if (curNode->activeJoinNodeListHead->next == NULL)
	{

		//curNode->activeJoinNodeListHead->next = oneNode;
		//oneNode->pre = curNode->activeJoinNodeListHead;
		curNode->activeJoinNodeListTail->next = oneNode;
		oneNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneNode;
#if DATASTRUCT
		//if (curNode->numOfActiveNode == 1)
		{
			oneListNode = (struct JoinNodeList*)malloc(sizeof(struct JoinNodeList));
			oneListNode->join = curNode;
			oneListNode->next = NULL;
			oneListNode->pre = NULL;
			flag = 1;
		}
#endif

	}
	else
	{
#if SCHEDULE
		struct activeJoinNode* p = curNode->activeJoinNodeListTail;
		struct fact* oneNodeFact = (struct fact*)theFact;
		while (p != curNode->activeJoinNodeListHead && ((p->currentPartialMatch == NULL && ((oneNodeFact->timestamp < ((struct fact*)p->theEntity)->timestamp) || 
			(p->currentPartialMatch != NULL && p->currentPartialMatch->l_timeStamp > oneNodeFact->timestamp))))){
			p = p->pre;
		}
		if (p == curNode->activeJoinNodeListHead) {
			oneNode->next = curNode->activeJoinNodeListHead->next;
			curNode->activeJoinNodeListHead->next = oneNode;
			oneNode->pre = curNode->activeJoinNodeListHead;
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneNode;
		}
		else{
			oneNode->next = p->next;
			if (p->next != NULL)p->next->pre = oneNode;
			p->next = oneNode; oneNode->pre = p;

			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneNode;
		}
#else 
		curNode->activeJoinNodeListTail->next = oneNode;
		oneNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneNode;
#endif
	}
	//curNode->activeJoinNodeListTail = oneNode;
	curNode->numOfActiveNode += 1;
#if CSECTION
	LeaveCriticalSection(&(curNode->nodeSection));
#endif

	if (flag){
#if CSECTION
		EnterCriticalSection(&g_cs);
#endif
		AddList(oneListNode);  //
#if CSECTION
		LeaveCriticalSection(&g_cs); 
#endif
	}

#else 
	if (activeNodeHead->next == NULL){ activeNodeHead->next = oneNode; oneNode->pre = activeNodeHead; }
	else
	{
		activeNodeTail->next = oneNode;
		oneNode->pre = activeNodeTail;
	} 
	activeNodeTail = oneNode;
#endif
#if THREAD
#if !CSECTION
	LeaveCriticalSection(&g_cs);
#endif
	
#if !MUTILTHREAD || REALMTHREAD
	ReleaseSemaphore(g_hSemaphoreBuffer, 1, NULL);
	totalAddActiveNode += 1;
#else if
	if (curNode->threadTag == 0){
		ReleaseSemaphore(g_hSemaphoreBufferOfThread1, 1, NULL);
	}
	else{
		ReleaseSemaphore(g_hSemaphoreBufferOfThread2, 1, NULL);
	}
#endif

#endif
}
globle void AddOneActiveNode(
	void* theEnv, 
	struct partialMatch* partialMatch,
	struct partialMatch* lhsBinds,
	struct partialMatch* rhsBinds,
	struct joinNode* curNode, 
	unsigned long hashValue,
	char whichEntry,
	int threadID)
{

	if (partialMatch == NULL){
		//printf("%d\n", whichEntry);
	}
	/**/
	LARGE_INTEGER large_time1;
	QueryPerformanceCounter(&large_time1);
	long long time1 = (long long)large_time1.QuadPart;
	/**/

	struct activeJoinNode *oneActiveNode = (struct activeJoinNode*) malloc(sizeof(struct activeJoinNode));
	oneActiveNode->curPMOnWhichSide = whichEntry;
	oneActiveNode->currentJoinNode = curNode;
	oneActiveNode->currentPartialMatch = partialMatch;
	oneActiveNode->lhsBinds = lhsBinds;
	oneActiveNode->rhsBinds = rhsBinds;
	oneActiveNode->hashValue = hashValue;
	oneActiveNode->hashOffset = hashValue;
	oneActiveNode->next = NULL;

	oneActiveNode->timeTag = partialMatch->timeTag;
	int flag = 0;
	struct JoinNodeList* oneListNode = NULL;
#if THREAD
#if !CSECTION
	EnterCriticalSection(&g_cs);
#else if
	EnterCriticalSection(&(curNode->nodeSection));
	//if(partialMatch != NULL)cur_partialmatch_time[1] = partialMatch->l_timeStamp;
#endif
#endif

#if SPEEDUP

	if (curNode->activeJoinNodeListHead->next == NULL)
	{
		//curNode->activeJoinNodeListHead->next = oneActiveNode;
		//oneActiveNode->pre = curNode->activeJoinNodeListHead;
		curNode->activeJoinNodeListTail->next = oneActiveNode;
		oneActiveNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneActiveNode;
#if DATASTRUCT
		//if (curNode->numOfActiveNode == 1)
		{
			oneListNode = (struct JoinNodeList*)malloc(sizeof(struct JoinNodeList));
			oneListNode->join = curNode;
			oneListNode->next = NULL;
			oneListNode->pre = NULL;
			flag = 1;
		}
#endif
	}
	else
	{
#if SCHEDULE
		struct activeJoinNode* p = curNode->activeJoinNodeListTail;
		/*if (curNode->activeJoinNodeListHead->next->currentPartialMatch->l_timeStamp < oneActiveNode->currentPartialMatch->l_timeStamp){
			oneActiveNode->next = curNode->activeJoinNodeListHead->next->next;
			curNode->activeJoinNodeListHead->next = oneActiveNode;
			oneActiveNode->pre = curNode->activeJoinNodeListHead->next;
			if(oneActiveNode->next != NULL)oneActiveNode->next->pre = oneActiveNode;

		}*/
		while (p != curNode->activeJoinNodeListHead && ((p->currentPartialMatch == NULL && (oneActiveNode->currentPartialMatch->l_timeStamp < ((struct fact*)p->theEntity)->timestamp) 
			|| (p->currentPartialMatch != NULL && p->currentPartialMatch->l_timeStamp > oneActiveNode->currentPartialMatch->l_timeStamp)))){
			p = p->pre;
		}
		if (p == curNode->activeJoinNodeListHead) {
			oneActiveNode->next = curNode->activeJoinNodeListHead->next;
			curNode->activeJoinNodeListHead->next = oneActiveNode;
			oneActiveNode->pre = curNode->activeJoinNodeListHead;
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneActiveNode;
		}
		else{
			oneActiveNode->next = p->next;
			if (p->next != NULL)p->next->pre = oneActiveNode;
			p->next = oneActiveNode; oneActiveNode->pre = p;
			
			if (p == curNode->activeJoinNodeListTail)
				curNode->activeJoinNodeListTail = oneActiveNode;
		}
#else 
		curNode->activeJoinNodeListTail->next = oneActiveNode;
		oneActiveNode->pre = curNode->activeJoinNodeListTail;
		curNode->activeJoinNodeListTail = oneActiveNode;
#endif
	}
	//curNode->activeJoinNodeListTail = oneActiveNode; 
	curNode->numOfActiveNode += 1;
#if CSECTION
	LeaveCriticalSection(&curNode->nodeSection);
#endif

	if (flag){
#if CSECTION
		EnterCriticalSection(&g_cs);
#endif
		AddList(oneListNode);
#if CSECTION
		LeaveCriticalSection(&g_cs);
#endif
	}
#else
	if (activeNodeHead->next == NULL)
	{
		activeNodeHead->next = oneActiveNode;
		oneActiveNode->pre = activeNodeHead;
		
	}
	else
	{
		activeNodeTail->next = oneActiveNode;
		oneActiveNode->pre = activeNodeTail;
		
	}
	activeNodeTail = oneActiveNode;
#endif
#if THREAD
#if !CSECTION
	LeaveCriticalSection(&g_cs);
#endif
	
#if !MUTILTHREAD ||REALMTHREAD
	ReleaseSemaphore(g_hSemaphoreBuffer, 1, NULL);
	totalAddActiveNode += 1;
#else if
	if (curNode->threadTag == 0){
		ReleaseSemaphore(g_hSemaphoreBufferOfThread1, 1, NULL);
	}
	else{
		ReleaseSemaphore(g_hSemaphoreBufferOfThread2, 1, NULL);
	}
#endif

#endif
	/**/
	LARGE_INTEGER large_time2;
	QueryPerformanceCounter(&large_time2);
	long long time2 = (long long)large_time2.QuadPart;
	cost_time[threadID] += (time2 - time1);
	/**/
	return;
}
unsigned int __stdcall MoveOnJoinNetworkThread(void *pM)
{
#if THREAD
	void *theEnv = ((struct ThreadNode*)pM)->theEnv;
	int threadID = ((struct ThreadNode*)pM)->threadTag;
#endif
	struct activeJoinNode *currentActiveNode;
	struct joinNode *currentJoinNode;
	struct partialMatch *currentPartialMatch;
	struct partialMatch *lhsBinds;
	struct partialMatch *rhsBinds;
	unsigned long hashValue;
	char enterDirection;
	struct fact *theFact;
	struct multifieldMarker *theMarks;
	struct patternNodeHeader *theHeader;
	unsigned long hashOffset;
	long long timeTag;

	LARGE_INTEGER large_time;
	LARGE_INTEGER start_time, end_time;

	int time_out = 1;
	while (1)
	{
		
		currentActiveNode = GetBestOneActiveNode(theEnv,threadID);
		

		if (currentActiveNode == NULL)continue;
		

		
		currentJoinNode = currentActiveNode->currentJoinNode;
		currentPartialMatch = currentActiveNode->currentPartialMatch;
		lhsBinds = currentActiveNode->lhsBinds;
		rhsBinds = currentActiveNode->rhsBinds;
		hashValue = currentActiveNode->hashValue;
		enterDirection = currentActiveNode->curPMOnWhichSide;
		theFact = (struct fact *) currentActiveNode->theEntity;
		theMarks = currentActiveNode->markers;
		theHeader = currentActiveNode->theHeader;
		//timeTag = currentActiveNode->timeTag;
		time_out = 1;
		QueryPerformanceCounter(&start_time);
		/*
		QueryPerformanceCounter(&large_time);
		//time_t end_time = time(NULL);
		long long time = (long long)large_time.QuadPart;
		
		long long PM_time=0;
		if (currentActiveNode->currentPartialMatch != NULL){
			PM_time = currentActiveNode->currentPartialMatch->l_timeStamp;
		}
		else
			PM_time = theFact->timestamp;
		if (time - PM_time > (2760000 * 97)){
			time_out = 0;
		}
		*/
		//printf("%d %d %d \n", currentJoinNode->depth, currentJoinNode->firstJoin, currentActiveNode->curPMOnWhichSide);

		if (time_out && currentJoinNode->firstJoin)
		{
			//printf("first\n");
			//currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			EnterCriticalSection(&g_move);
			currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			//currentPartialMatch = theFact->alphaMatch;
			LeaveCriticalSection(&g_move);
			currentPartialMatch->owner = theHeader;
			//currentPartialMatch->timeTag = timeTag;

			((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			EmptyDrive(theEnv, currentJoinNode, currentPartialMatch,threadID);
		}
		else if (time_out && currentActiveNode->curPMOnWhichSide == LHS)
		{
			
			UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			//currentPartialMatch->hashValue = hashValue;
			NetworkAssertLeft(theEnv, currentPartialMatch, currentJoinNode,threadID);
		
		}
		else if (time_out && currentActiveNode->curPMOnWhichSide == RHS)
		{
			//UpdateBetaPMLinks(theEnv, currentPartialMatch, lhsBinds, rhsBinds, currentJoinNode, hashValue, enterDirection);
			EnterCriticalSection(&g_move);
			//currentPartialMatch = CreateAlphaMatch(theEnv, theFact, theMarks, theHeader, currentActiveNode->hashOffset);

			currentPartialMatch = CreateAlphaMatch(GetEnvironmentByIndex(1), theFact, theMarks, theHeader, currentActiveNode->hashOffset);
			//currentPartialMatch = theFact->alphaMatch;
			LeaveCriticalSection(&g_move);

			currentPartialMatch->owner = theHeader;
#if THREAD 1
			//EnterCriticalSection(&g_move); // remove by xuchao , remove ok?
			//EnterCriticalSection(&g_fact_join);
#if OPTIMIZE
			theFact->factNotOnNodeMask = (theFact->factNotOnNodeMask | (1<<currentJoinNode->depth));

#else
			struct factNotOnJoinNode *p = theFact->factNotOnNode;
			while (p->next != NULL){
				if (p->next->join == currentJoinNode){
					struct factNotOnJoinNode* tmp = p->next;
					p->next = p->next->next;
					tmp->next = NULL;
					break;
				}
				p = p->next;
			}
#endif
			//LeaveCriticalSection(&g_fact_join);
			//LeaveCriticalSection(&g_move);
#endif
		    ((struct patternMatch *)theFact->list)->theMatch = currentPartialMatch;
			NetworkAssertRight(theEnv, currentPartialMatch, currentJoinNode,threadID);
			//LeaveCriticalSection(&g_move);
			//LeaveCriticalSection(&g_cs);
		}
		else {/*error*/}

#if REALMTHREAD

		//EnterCriticalSection(&g_cs);

		currentJoinNode->threadTag = 0;
		free(currentActiveNode);
		currentActiveNode = NULL;
		
		QueryPerformanceCounter(&end_time);
		run_time[threadID] += (end_time.QuadPart - start_time.QuadPart);
		//LeaveCriticalSection(&g_cs);
#endif
	}
	
	return 0;
}
#endif // THREAD
globle double SlowDown()
{
	//Sleep(3000);
	int slow = 5;
	int alpha = 123;
	double result = 1;
	while (slow-- > 0){
		result = sin(alpha * result);
	}
	return result;
}
