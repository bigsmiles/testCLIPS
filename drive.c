   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.30  10/19/06            */
   /*                                                     */
   /*                    DRIVE MODULE                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Handles join network activity associated with    */
/*   with the addition of a data entity such as a fact or    */
/*   instance.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Removed INCREMENTAL_RESET and                  */
/*            LOGICAL_DEPENDENCIES compilation flags.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Rule with exists CE has incorrect activation.  */
/*            DR0867                                         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added additional developer statistics to help  */
/*            analyze join network performance.              */
/*                                                           */
/*            Removed pseudo-facts used in not CE.           */
/*                                                           */
/*************************************************************/

#define _DRIVE_SOURCE_

#include <stdio.h>
#define _STDIO_INCLUDED_
#include <stdlib.h>

#include "setup.h"

#if DEFRULE_CONSTRUCT
//#include <windows.h>

#include "agenda.h"
#include "constant.h"
#include "engine.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "reteutil.h"
#include "retract.h"
#include "router.h"
#include "lgcldpnd.h"
#include "incrrset.h"

#include "drive.h"  
#include "factmngr.h"
#include "move.h"
  
/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   void                    EmptyDrive(void *,struct joinNode *,struct partialMatch *,int); // change static by xuchao
   static void                    JoinNetErrorMessage(void *,struct joinNode *);
   

   //add by xuchao
   extern CRITICAL_SECTION g_runDebug;
   extern CRITICAL_SECTION g_move; 
   extern long long search_time;
   extern long long cur_partialmatch_time[3];
   extern FILE* print_file[3];
#if SLIDING_WINDOW
//#define WINDOW_SIZE (2760000*10)
#define WINDOW_SIZE (5)
#define FREEPM 1
#endif
/************************************************/
/* NetworkAssert: Primary routine for filtering */
/*   a partial match through the join network.  */
/************************************************/

globle void NetworkAssert(
  void *theEnv,
  struct partialMatch *binds,
  struct joinNode *join)
  {
   /*=========================================================*/
   /* If an incremental reset is being performed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/
	//added for test...
	//printf("in drive.c join->netWorkTest->value: %s\n", (char*)join->networkTest->value);

#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->IncrementalResetInProgress && (join->initialize == FALSE)) return;
#endif

   /*==================================================*/
   /* Use a special routine if this is the first join. */
   /*==================================================*/

   if (join->firstJoin)
     {
      EmptyDrive(theEnv,join,binds,0);
      return;
     }

   /*================================*/
   /* Enter the join from the right. */
   /*================================*/

   NetworkAssertRight(theEnv,binds,join,0);

   return;
  }

/*****************************************************/
/* NetworkAssertRight: Primary routine for filtering */
/*   a partial match through the join network from   */
/*   the RHS of a join.                              */
/*****************************************************/
globle void NetworkAssertRight(
  void *theEnv,
  struct partialMatch *rhsBinds,
  struct joinNode *join,
  int threadID)
  {
   struct partialMatch *lhsBinds, *nextBind;
   int exprResult, restore = FALSE;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;

   /*=========================================================*/
   /* If an incremental reset is being performed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/
#if SLOW
   SlowDown();//add by xuchao
#endif
#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->IncrementalResetInProgress && (join->initialize == FALSE)) return;
#endif

   if (join->firstJoin)
     {
      EmptyDrive(theEnv,join,rhsBinds,threadID);
      return;
     }
  
   /*=====================================================*/
   /* The partial matches entering from the LHS of a join */
   /* are stored in the left beta memory of the join.     */
   /*=====================================================*/


   lhsBinds = GetLeftBetaMemory(join,rhsBinds->hashValue);

#if DEVELOPER
   if (lhsBinds != NULL)
     { EngineData(theEnv)->rightToLeftLoops++; }
#endif

   /*====================================*/
   /* Set up the evaluation environment. */
   /*====================================*/
   
   if (lhsBinds != NULL)
     {
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
      EngineData(theEnv)->GlobalJoin = join;
      restore = TRUE;
     }
    
   /*===================================================*/
   /* Compare each set of binds on the opposite side of */
   /* the join with the set of binds that entered this  */
   /* join. If the binds don't mismatch, then perform   */
   /* the appropriate action for the logic of the join. */
   /*===================================================*/
#if DROOLS_WINDOW
   LARGE_INTEGER cur_time;
   QueryPerformanceCounter(&cur_time);
   long long cur_system_time = (long long)cur_time.QuadPart;
   cur_system_time = rhsBinds->l_timeStamp;
   
   /**/
   if (cur_system_time - rhsBinds->l_timeStamp > WINDOW_SIZE){
	   lhsBinds = NULL;
   }
   /**/

   long long fix_l_time = rhsBinds->r_timeStamp - WINDOW_SIZE;
   long long fix_r_time = rhsBinds->l_timeStamp + WINDOW_SIZE;
   long long rhsBinds_l_time = rhsBinds->l_timeStamp;
   long long rhsBinds_r_time = rhsBinds->r_timeStamp;
   long long lhsBinds_l_time;
   long long lhsBinds_r_time;
#endif

   while (lhsBinds != NULL)
     {
      nextBind = lhsBinds->nextInMemory;
#if SLIDING_WINDOW

	  lhsBinds_l_time = lhsBinds->l_timeStamp;
	  lhsBinds_r_time = lhsBinds->r_timeStamp;

	  
#if DROOLS_WINDOW
	  if (cur_system_time - lhsBinds_l_time > WINDOW_SIZE)
#else if
	  if ((lhsBinds_l_time < fix_l_time) || (lhsBinds_r_time > fix_r_time))
#endif 
	  //if (llabs(lhsBinds_l_time - rhsBinds_r_time) > WINDOW_SIZE || llabs(lhsBinds_r_time - rhsBinds_l_time) > WINDOW_SIZE)
	  {
		  //printf("over window size\n");
		  struct partialMatch* tmp = lhsBinds;
		  struct partialMatch* pre = lhsBinds->prevInMemory;
		  lhsBinds = nextBind;
		  //free this PM tmp;
		  /**/
#if FREEPM
		  //if (lhsBinds_r_time > fix_r_time)break; //add 8-10
		  //if (lhsBinds_r_time > rhsBinds_l_time)continue;
		  
		  //if (lhsBinds_r_time > fix_l_time) continue; //drools_window

		  if (pre != NULL){
			  pre->nextInMemory = lhsBinds;
			  if (lhsBinds != NULL){
				  lhsBinds->prevInMemory = pre;
			  }
			  else{
				  join->leftMemory->beta_last[tmp->hashValue % join->leftMemory->size] = pre;
			  }
		  }
		  else{
			  join->leftMemory->beta[tmp->hashValue % join->leftMemory->size] = lhsBinds;
			  if(lhsBinds != NULL)
				  lhsBinds->prevInMemory = pre; //NULL
			  else{
				  join->leftMemory->beta_last[tmp->hashValue % join->leftMemory->size] = lhsBinds;
			  }
		  }
		  EnterCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
		  rtn_var_struct(tmp->whichEnv, partialMatch, (int) sizeof(struct genericMatch*) * (tmp->bcount - 1), tmp);
		  LeaveCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
#endif
		  /**/
		  continue;
	  }
#endif
      join->memoryCompares++;
      
      /*===========================================================*/
      /* Initialize some variables pointing to the partial matches */
      /* in the LHS and RHS of the join.                           */
      /*===========================================================*/

      if (lhsBinds->hashValue != rhsBinds->hashValue)
        {
#if DEVELOPER
         if (join->leftMemory->size == 1)
           { EngineData(theEnv)->betaHashListSkips++; }
         else
           { EngineData(theEnv)->betaHashHTSkips++; }
           
         if (lhsBinds->marker != NULL)
           { EngineData(theEnv)->unneededMarkerCompare++; }
#endif
         lhsBinds = nextBind;
         continue;
        }
        
      /*===============================================================*/
      /* If there already is an associated RHS partial match stored in */
      /* the LHS partial match from the beta memory of this join, then */
      /* the exists/nand CE has already been satisfied and we can move */
      /* on to the next partial match found in the beta memory.        */
      /*===============================================================*/
        
      if (lhsBinds->marker != NULL)
        { 
#if DEVELOPER
         EngineData(theEnv)->unneededMarkerCompare++;
#endif
         lhsBinds = nextBind;
         continue;
        }

      /*===================================================*/
      /* If the join has no expression associated with it, */
      /* then the new partial match derived from the LHS   */
      /* and RHS partial matches is valid.                 */
      /*===================================================*/

      if (join->networkTest == NULL)
        { exprResult = TRUE; }

      /*=========================================================*/
      /* If the join has an expression associated with it, then  */
      /* evaluate the expression to determine if the new partial */
      /* match derived from the LHS and RHS partial matches is   */
      /* valid (i.e. variable bindings are consistent and        */
      /* predicate expressions evaluate to TRUE).                */
      /*=========================================================*/

      else
        {
#if DEVELOPER
         EngineData(theEnv)->rightToLeftComparisons++;
#endif
         EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
         exprResult = EvaluateJoinExpression(theEnv,join->networkTest,join);
         if (EvaluationData(theEnv)->EvaluationError)
           {
            if (join->patternIsNegated) exprResult = TRUE;
            SetEvaluationError(theEnv,FALSE);
           }

#if DEVELOPER
         if (exprResult)
           { EngineData(theEnv)->rightToLeftSucceeds++; }
#endif
        }

      if ((join->secondaryNetworkTest != NULL) && exprResult)
        {
         /* EngineData(theEnv)->GlobalRHSBinds = NULL; */
         
         exprResult = EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
         if (EvaluationData(theEnv)->EvaluationError)
           { SetEvaluationError(theEnv,FALSE); }
        }

      /*====================================================*/
      /* If the join expression evaluated to TRUE (i.e.     */
      /* there were no conflicts between variable bindings, */
      /* all tests were satisfied, etc.), then perform the  */
      /* appropriate action given the logic of this join.   */
      /*====================================================*/

      if (exprResult != FALSE)
        {
         if (join->patternIsExists)
           {
            AddBlockedLink(lhsBinds,rhsBinds);
            PPDrive(theEnv,lhsBinds,NULL,join,threadID);
           }
         else if (join->patternIsNegated || join->joinFromTheRight)
           {
            AddBlockedLink(lhsBinds,rhsBinds);
            if (lhsBinds->children != NULL)
              { PosEntryRetractBeta(theEnv,lhsBinds,lhsBinds->children); }
            /*
            if (lhsBinds->dependents != NULL) 
              { RemoveLogicalSupport(theEnv,lhsBinds); }
            */
           } 
         else
           { PPDrive(theEnv,lhsBinds,rhsBinds,join,threadID); }
        }

      /*====================================*/
      /* Move on to the next partial match. */
      /*====================================*/

      lhsBinds = nextBind;
     }

   /*=========================================*/
   /* Restore the old evaluation environment. */
   /*=========================================*/

   if (restore)
     {
      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;
     }
     
   return;
  }

/****************************************************/
/* NetworkAssertLeft: Primary routine for filtering */
/*   a partial match through the join network when  */
/*   entering through the left side of a join.      */
/****************************************************/
globle void NetworkAssertLeft(
  void *theEnv,
  struct partialMatch *lhsBinds,
  struct joinNode *join,
  int threadID)
  {
   struct partialMatch *rhsBinds;
   int exprResult, restore = FALSE;
   unsigned long entryHashValue;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;
   int rhsBindsIsFact = FALSE;
#if SLIDING_WINDOW
   struct alphaMemoryHash *alphaHash;
#endif

   /*=========================================================*/
   /* If an incremental reset is being performed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/
#if SLOW
   SlowDown();//add by xuchao
#endif
#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->IncrementalResetInProgress && (join->initialize == FALSE)) return;
#endif

   /*===================================*/
   /* The only action for the last join */
   /* of a rule is to activate it.      */
   /*===================================*/
   
   if (join->ruleToActivate != NULL)
     {
#if THREAD
	   //void *rule = EnvFindDefRule(theEnv,EnvGetDefruleName(GetEnvironmentByIndex(0), join->ruleToActivate));
	   //if (search_time++ < 4200000)return;
	   //EnterCriticalSection(&g_runDebug);
	   LARGE_INTEGER large_time;
	   
	   QueryPerformanceCounter(&large_time);
	   //time_t end_time = time(NULL);
	   long long time = (long long)large_time.QuadPart;
	   long long l_time = 0,r_time = 0;
#if SLIDING_WINDOW
	   l_time = lhsBinds->l_timeStamp;
	   r_time = lhsBinds->r_timeStamp;
#endif
	   search_time = time;
	   
	   //fprintf(print_file[ThreadID],"%s %lld \n", join->ruleToActivate->header.name->contents,time);
	   
	   printf("%s %lld \n", join->ruleToActivate->header.name->contents, time);
	  
	   //printf("%lld \n", time);
	   //LeaveCriticalSection(&g_runDebug);
	   /*
	   AddActivation(theEnv, EnvFindDefrule(theEnv, EnvGetDefruleName(GetEnvironmentByIndex(0), join->ruleToActivate)), lhsBinds);
	   
	   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
	   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
	   EnterCriticalSection(&g_move);
	   EnvRun(theEnv, -1); //add by xucaho
	   LeaveCriticalSection(&g_move);
	   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
	   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
	   */
#else if
      AddActivation(theEnv,join->ruleToActivate,lhsBinds);
#endif
      return;
     }

   /*==================================================*/
   /* Initialize some variables used to indicate which */
   /* side is being compared to the new partial match. */
   /*==================================================*/

   entryHashValue = lhsBinds->hashValue;
   if (join->joinFromTheRight)
     { rhsBinds = GetRightBetaMemory(join,entryHashValue); }
   else
   {
	   rhsBinds = GetAlphaMemory(GetEnvironmentByIndex(1), (struct patternNodeHeader *) join->rightSideEntryStructure, entryHashValue);
#if SLIDING_WINDOW
	   alphaHash = GetAlphaMemoryHash(GetEnvironmentByIndex(1), (struct patternNodeHeader *) join->rightSideEntryStructure, entryHashValue);
#endif
	   //rhsBinds = GetAlphaMemory(theEnv, (struct patternNodeHeader *) join->rightSideEntryStructure, entryHashValue); 
#if THREAD
	   //add by xuchao
	   rhsBindsIsFact = TRUE;
#endif
   }
       
#if DEVELOPER
   if (rhsBinds != NULL)
     { EngineData(theEnv)->leftToRightLoops++; }
#endif

  
   
   /*====================================*/
   /* Set up the evaluation environment. */
   /*====================================*/
   
   if ((rhsBinds != NULL) || (join->secondaryNetworkTest != NULL))
     {
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
      EngineData(theEnv)->GlobalJoin = join;
      restore = TRUE;
     }
  
   /*===================================================*/
   /* Compare each set of binds on the opposite side of */
   /* the join with the set of binds that entered this  */
   /* join. If the binds don't mismatch, then perform   */
   /* the appropriate action for the logic of the join. */
   /*===================================================*/

   //add by xuchao for REALMTHREA
   //printf("leftrhs: %d %d %d\n", rhsBindsIsFact,rhsBindsIsFact, rhsBinds == NULL ? 0 : 1);
   int garbegeRef = FALSE;

#if DROOLS_WINDOW
   LARGE_INTEGER cur_time;
   QueryPerformanceCounter(&cur_time);
   long long cur_system_time = (long long)cur_time.QuadPart;
   cur_system_time = lhsBinds->l_timeStamp;
   /**/
   if (cur_system_time - lhsBinds->l_timeStamp > WINDOW_SIZE){
	   rhsBinds = NULL;
   }
  
   /**/
   /**/
   long long l_time = lhsBinds->l_timeStamp;
   long long r_time = lhsBinds->r_timeStamp;
   long long fix_l_time = r_time - WINDOW_SIZE;
   long long fix_r_time = l_time + WINDOW_SIZE;
   /**/
   long long factTime;
#endif

   while (rhsBinds != NULL)
     {

#if THREAD
	   //add by xuchao
	   
	   if (rhsBindsIsFact){
		   struct fact *curFact = (struct fact*)(rhsBinds->binds[0].gm.theMatch->matchingItem);
		   int flag = 0;
#if OPTIMIZE
		   if(((1 << join->depth) & curFact->factNotOnNodeMask) == 0){
			   flag = 1;
		   }
#else
		   struct factNotOnJoinNode* p = curFact->factNotOnNode;
		  
		   //EnterCriticalSection(&g_move);
		   while (p!= NULL){
			   if (p->join == join){
				   flag = 1; 
				   break;
			   }
			   p = p->next;
			  
		   }
#endif
		   //LeaveCriticalSection(&g_move);
#if SLIDING_WINDOW

		   //QueryPerformanceCounter(&cur_time);
		   //cur_system_time = (long long)cur_time.QuadPart;

		   factTime = curFact->timestamp;
		  
		   //if (llabs(factTime - l_time) > WINDOW_SIZE || llabs(r_time - factTime) > WINDOW_SIZE)
#if DROOLS_WINDOW
		   if(cur_system_time - factTime > WINDOW_SIZE)
#else if
		   if ((factTime < fix_l_time || factTime > fix_r_time))
#endif
		   {
			   
			   struct partialMatch* tmp = rhsBinds;
			   struct partialMatch* pre = rhsBinds->prevInMemory;
			   rhsBinds = rhsBinds->nextInMemory;
			   //free this PM tmp;
			   /**/
			  
#if FREEPM  
#if !DROOLS_WINDOW
			   //if (factTime > fix_r_time)  break;//8-23
			   int useable = TRUE;
			   long long may_be_min_time = l_time;
			   struct partialMatch* left_memory = GetLeftBetaMemory(join, lhsBinds->hashValue);
			   if (left_memory != NULL) may_be_min_time = min(may_be_min_time, left_memory->l_timeStamp);
			   EnterCriticalSection(&join->nodeSection);
			   if (join->numOfActiveNode > 0 && join->activeJoinNodeListHead->next->currentPartialMatch != NULL){
				   may_be_min_time = min(may_be_min_time, join->activeJoinNodeListHead->next->currentPartialMatch->l_timeStamp);
			   }
			   if (join->numOfActiveNode > 0 && join->activeJoinNodeListHead->next->currentPartialMatch == NULL){
				   may_be_min_time = min(may_be_min_time, ((struct fact*)(join->activeJoinNodeListHead->next->theEntity))->timestamp);
			   }
			   //if (join->numOfActiveNode > 0 && join->activeJoinNodeListTail->currentPartialMatch != NULL)
				//   may_be_min_time = min(may_be_min_time, join->activeJoinNodeListTail->currentPartialMatch->l_timeStamp);
			   
			   LeaveCriticalSection(&join->nodeSection);
			   struct joinNode* last_level_join = join->lastLevel;
			   //struct joinNode* last_level_join = join;
			   /**/
			   long long x_alpha = 9999999999999;
			   //while (last_level_join)
			   {
				   unsigned long hashoff = entryHashValue;
				   //unsigned long hashoff = ComputeRightHashValue(GetEnvironmentByIndex(1), (struct patternNodeHeader *) last_level_join->rightSideEntryStructure);
				   
				   struct partialMatch* x_PM  = GetAlphaMemory(GetEnvironmentByIndex(1), (struct patternNodeHeader *) last_level_join->rightSideEntryStructure, hashoff);
				   x_alpha = 9999999999999;
				   if(x_PM != NULL) x_alpha= x_PM->l_timeStamp;
				   long long x_beta = 9999999999999;
				   EnterCriticalSection(&last_level_join->nodeSection);
				   //if (last_level_join->threadTag == -1)
				   {
					   may_be_min_time = min(may_be_min_time, (min(cur_partialmatch_time[1], cur_partialmatch_time[2])));
				   }
				   /**
				   if (last_level_join->numOfActiveNode > 0){//last_level_join->threadTag != -1 && 
					   if (last_level_join->activeJoinNodeListHead->next->currentPartialMatch != NULL)
						   x_beta = last_level_join->activeJoinNodeListHead->next->currentPartialMatch->l_timeStamp;
					   else
						   x_beta = ((struct fact*)(last_level_join->activeJoinNodeListHead->next->theEntity))->timestamp;
					   
				   }
				   */
				   LeaveCriticalSection(&last_level_join->nodeSection);
				   may_be_min_time = min(may_be_min_time, min(x_alpha,x_beta));
				   last_level_join = last_level_join->lastLevel;
			   }
			   /**/
			 
			   //if (factTime > fix_l_time)
			   if (factTime > may_be_min_time - WINDOW_SIZE)
			   //if (factTime > l_time - WINDOW_SIZE)
			   {
				   //break; 
				   continue; 
			   }

			   EnterCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
			   /**/
			   int mask = 0;
			   struct patternNodeHeader* theHeader = (struct patternNodeHeader *) join->rightSideEntryStructure;
			   for (struct joinNode* listOfJoins = theHeader->entryJoin;
				   listOfJoins != NULL;
				   listOfJoins = listOfJoins->rightMatchNode){
				   
				   if (listOfJoins == join)break;
				   mask += 1;
			   }
			   if (tmp->refMask & (1<<mask)){
				   tmp->refCount -= 1;
				   tmp->refMask &= ~(1 << mask);
			   }
			   if (tmp->refCount > 0)
			   {
				   LeaveCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
				   continue;
			   }
#endif // DROOLS_WINDOW
			   /**/
			   // else free tmp to memory pool
			   if (pre != NULL){
				   /**/
				   pre->nextInMemory = rhsBinds;
				   if (rhsBinds != NULL)
					   rhsBinds->prevInMemory = pre;
				   else
					   alphaHash->endOfQueue = pre;
					  /* */
			   }
			   else
			   {
				   /**/
				   if (alphaHash != NULL){
					   //printf("NULL alpha\n");
					   alphaHash->alphaMemory = rhsBinds;
					   //alphaHash->endOfQueue = rhsBinds;
				   }
				   if (rhsBinds != NULL)
					   rhsBinds->prevInMemory = pre;//NULL
				   //else
					   alphaHash->endOfQueue = pre;
				  /**/
					   
			   }
			   EnterCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
			   rtn_var_struct(tmp->whichEnv, partialMatch, (int) sizeof(struct genericMatch*) * (tmp->bcount - 1), tmp);
			   LeaveCriticalSection(&(MemoryData(tmp->whichEnv)->memoSection));
			   
#endif
			   /**/

			   continue;
		   }
#endif
		   if (flag){
			   rhsBinds = rhsBinds->nextInMemory;
			   continue;
		   }
		   
	   }
	  
#endif	   
      join->memoryCompares++;
	  //add by xuchao for REALMTHREA
	 //printf("left memo: %d\n", join->memoryCompares++);

      /*===================================================*/
      /* If the join has no expression associated with it, */
      /* then the new partial match derived from the LHS   */
      /* and RHS partial matches is valid.                 */
      /*===================================================*/

      if (join->networkTest == NULL)
        { exprResult = TRUE; }

      /*=========================================================*/
      /* If the join has an expression associated with it, then  */
      /* evaluate the expression to determine if the new partial */
      /* match derived from the LHS and RHS partial matches is   */
      /* valid (i.e. variable bindings are consistent and        */
      /* predicate expressions evaluate to TRUE).                */
      /*=========================================================*/

      else
        {
#if DEVELOPER
         EngineData(theEnv)->leftToRightComparisons++;
#endif
		 
         EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
         
         exprResult = EvaluateJoinExpression(theEnv,join->networkTest,join);
         if (EvaluationData(theEnv)->EvaluationError)
           {
            if (join->patternIsNegated) exprResult = TRUE;
            SetEvaluationError(theEnv,FALSE);
           }

#if DEVELOPER
         if (exprResult)
           { EngineData(theEnv)->leftToRightSucceeds++; }
#endif
        } 

      /*====================================================*/
      /* If the join expression evaluated to TRUE (i.e.     */
      /* there were no conflicts between variable bindings, */
      /* all tests were satisfied, etc.), then perform the  */
      /* appropriate action given the logic of this join.   */
      /*====================================================*/

      if (exprResult != FALSE)
        {
         /*==============================================*/
         /* Use the PPDrive routine when the join isn't  */
         /* associated with a not CE and it doesn't have */
         /* a join from the right.                       */ 
         /*==============================================*/

         if ((join->patternIsNegated == FALSE) &&
             (join->patternIsExists == FALSE) &&
             (join->joinFromTheRight == FALSE))
           { PPDrive(theEnv,lhsBinds,rhsBinds,join,threadID); }

         /*==================================================*/
         /* At most, one partial match will be generated for */
         /* a match from the right memory of an exists CE.   */
         /*==================================================*/
         
         else if (join->patternIsExists)
           { 
            AddBlockedLink(lhsBinds,rhsBinds);
            PPDrive(theEnv,lhsBinds,NULL,join,threadID);
            EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
            EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
            EngineData(theEnv)->GlobalJoin = oldJoin;
            return;
           }
           
         /*===========================================================*/
         /* If the new partial match entered from the LHS of the join */
         /* and the join is either associated with a not CE or the    */
         /* join has a join from the right, then mark the LHS partial */
         /* match indicating that there is a RHS partial match        */
         /* preventing this join from being satisfied. Once this has  */
         /* happened, the other RHS partial matches don't have to be  */
         /* tested since it only takes one partial match to prevent   */
         /* the LHS from being satisfied.                             */
         /*===========================================================*/

         else
           {
            AddBlockedLink(lhsBinds,rhsBinds);
            break;
           }
        }

      /*====================================*/
      /* Move on to the next partial match. */
      /*====================================*/

      rhsBinds = rhsBinds->nextInMemory;
     }

   /*==================================================================*/
   /* If a join with an associated not CE or join from the right was   */
   /* entered from the LHS side of the join, and the join expression   */
   /* failed for all sets of matches for the new bindings on the LHS   */
   /* side (there was no RHS partial match preventing the LHS partial  */
   /* match from being satisfied), then the LHS partial match appended */
   /* with an pseudo-fact that represents the instance of the not      */
   /* pattern or join from the right that was satisfied should be sent */
   /* to the joins below this join.                                    */
   /*==================================================================*/

   if ((join->patternIsNegated || join->joinFromTheRight) && 
       (! join->patternIsExists) &&
       (lhsBinds->marker == NULL))
     {
      if (join->secondaryNetworkTest != NULL)
        {
         EngineData(theEnv)->GlobalRHSBinds = NULL;
         
         exprResult = EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
         if (EvaluationData(theEnv)->EvaluationError)
           { SetEvaluationError(theEnv,FALSE); }
           
         if (exprResult)
            { PPDrive(theEnv,lhsBinds,NULL,join,threadID); }
		}
      else
        { PPDrive(theEnv,lhsBinds,NULL,join,threadID); } 
     }

   /*=========================================*/
   /* Restore the old evaluation environment. */
   /*=========================================*/

   if (restore)
     {
      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;
     }

   return;
  }

/*******************************************************/
/* EvaluateJoinExpression: Evaluates join expressions. */
/*   Performs a faster evaluation for join expressions */
/*   than if EvaluateExpression was used directly.     */
/*******************************************************/
globle intBool EvaluateJoinExpression(
  void *theEnv,
  struct expr *joinExpr,
  struct joinNode *joinPtr)
  {
   DATA_OBJECT theResult;
   int andLogic, result = TRUE;

   /*======================================*/
   /* A NULL expression evaluates to TRUE. */
   /*======================================*/

   if (joinExpr == NULL) return(TRUE);

   /*====================================================*/
   /* Initialize some variables which allow this routine */
   /* to avoid calling the "and" and "or" functions if   */
   /* they are the first part of the expression to be    */
   /* evaluated. Most of the join expressions do not use */
   /* deeply nested and/or functions so this technique   */
   /* speeds up evaluation.                              */
   /*====================================================*/

   if (joinExpr->value == ExpressionData(theEnv)->PTR_AND)
     {
      andLogic = TRUE;
      joinExpr = joinExpr->argList;
     }
   else if (joinExpr->value == ExpressionData(theEnv)->PTR_OR)
     {
      andLogic = FALSE;
      joinExpr = joinExpr->argList;
     }
   else
     { andLogic = TRUE; }

   /*=========================================*/
   /* Evaluate each of the expressions linked */
   /* together in the join expression.        */
   /*=========================================*/

   while (joinExpr != NULL)
     {
      /*================================*/
      /* Evaluate a primitive function. */
      /*================================*/

      if ((EvaluationData(theEnv)->PrimitivesArray[joinExpr->type] == NULL) ?
          FALSE :
          EvaluationData(theEnv)->PrimitivesArray[joinExpr->type]->evaluateFunction != NULL)
        {
         struct expr *oldArgument;

         oldArgument = EvaluationData(theEnv)->CurrentExpression;
         EvaluationData(theEnv)->CurrentExpression = joinExpr;
         result = (*EvaluationData(theEnv)->PrimitivesArray[joinExpr->type]->evaluateFunction)(theEnv,joinExpr->value,&theResult);
         EvaluationData(theEnv)->CurrentExpression = oldArgument;
        }

      /*=============================*/
      /* Evaluate the "or" function. */
      /*=============================*/

      else if (joinExpr->value == ExpressionData(theEnv)->PTR_OR)
        {
         result = FALSE;
         if (EvaluateJoinExpression(theEnv,joinExpr,joinPtr) == TRUE)
           {
            if (EvaluationData(theEnv)->EvaluationError)
              { return(FALSE); }
            result = TRUE;
           }
         else if (EvaluationData(theEnv)->EvaluationError)
           { return(FALSE); }
        }

      /*==============================*/
      /* Evaluate the "and" function. */
      /*==============================*/

      else if (joinExpr->value == ExpressionData(theEnv)->PTR_AND)
        {
         result = TRUE;
         if (EvaluateJoinExpression(theEnv,joinExpr,joinPtr) == FALSE)
           {
            if (EvaluationData(theEnv)->EvaluationError)
              { return(FALSE); }
            result = FALSE;
           }
         else if (EvaluationData(theEnv)->EvaluationError)
           { return(FALSE); }
        }

      /*==========================================================*/
      /* Evaluate all other expressions using EvaluateExpression. */
      /*==========================================================*/

      else
        {
         EvaluateExpression(theEnv,joinExpr,&theResult);

         if (EvaluationData(theEnv)->EvaluationError)
           {
            JoinNetErrorMessage(theEnv,joinPtr);
            return(FALSE);
           }

         if ((theResult.value == EnvFalseSymbol(theEnv)) && (theResult.type == SYMBOL))
           { result = FALSE; }
         else
           { result = TRUE; }
        }

      /*====================================*/
      /* Handle the short cut evaluation of */
      /* the "and" and "or" functions.      */
      /*====================================*/

      if ((andLogic == TRUE) && (result == FALSE))
        { return(FALSE); }
      else if ((andLogic == FALSE) && (result == TRUE))
        { return(TRUE); }

      /*==============================================*/
      /* Move to the next expression to be evaluated. */
      /*==============================================*/

      joinExpr = joinExpr->nextArg;
     }

   /*=================================================*/
   /* Return the result of evaluating the expression. */
   /*=================================================*/

   return(result);
  }

/*******************************************************/
/* EvaluateSecondaryNetworkTest:     */
/*******************************************************/
globle intBool EvaluateSecondaryNetworkTest(
  void *theEnv,
  struct partialMatch *leftMatch,
  struct joinNode *joinPtr)
  {
   int joinExpr;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;

   if (joinPtr->secondaryNetworkTest == NULL)
     { return(TRUE); }
        
#if DEVELOPER
   EngineData(theEnv)->rightToLeftComparisons++;
#endif
   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
   oldJoin = EngineData(theEnv)->GlobalJoin;
   EngineData(theEnv)->GlobalLHSBinds = leftMatch;
   EngineData(theEnv)->GlobalRHSBinds = NULL;
   EngineData(theEnv)->GlobalJoin = joinPtr;

   joinExpr = EvaluateJoinExpression(theEnv,joinPtr->secondaryNetworkTest,joinPtr);
   EvaluationData(theEnv)->EvaluationError = FALSE;

   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
   EngineData(theEnv)->GlobalJoin = oldJoin;

   return(joinExpr);
  }

/*******************************************************/
/* BetaMemoryHashValue:     */
/*******************************************************/
globle unsigned long BetaMemoryHashValue(
  void *theEnv,
  struct expr *hashExpr,
  struct partialMatch *lbinds,
  struct partialMatch *rbinds,
  struct joinNode *joinPtr)
  {
   DATA_OBJECT theResult;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;
   unsigned long hashValue = 0;
   unsigned long multiplier = 1;
   
   /*======================================*/
   /* A NULL expression evaluates to zero. */
   /*======================================*/

   if (hashExpr == NULL) return(0);

   /*=========================================*/
   /* Initialize some of the global variables */
   /* used when evaluating expressions.       */
   /*=========================================*/

   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
   oldJoin = EngineData(theEnv)->GlobalJoin;
   EngineData(theEnv)->GlobalLHSBinds = lbinds;
   EngineData(theEnv)->GlobalRHSBinds = rbinds;
   EngineData(theEnv)->GlobalJoin = joinPtr;

   /*=========================================*/
   /* Evaluate each of the expressions linked */
   /* together in the join expression.        */
   /*=========================================*/

   while (hashExpr != NULL)
     {
      /*================================*/
      /* Evaluate a primitive function. */
      /*================================*/

      if ((EvaluationData(theEnv)->PrimitivesArray[hashExpr->type] == NULL) ?
          FALSE :
          EvaluationData(theEnv)->PrimitivesArray[hashExpr->type]->evaluateFunction != NULL)
        {
         struct expr *oldArgument;

         oldArgument = EvaluationData(theEnv)->CurrentExpression;
         EvaluationData(theEnv)->CurrentExpression = hashExpr;
         (*EvaluationData(theEnv)->PrimitivesArray[hashExpr->type]->evaluateFunction)(theEnv,hashExpr->value,&theResult);
         EvaluationData(theEnv)->CurrentExpression = oldArgument;
        }

      /*==========================================================*/
      /* Evaluate all other expressions using EvaluateExpression. */
      /*==========================================================*/

      else
        { EvaluateExpression(theEnv,hashExpr,&theResult); }

      switch (theResult.type)
        {
         case STRING:
         case SYMBOL:
         case INSTANCE_NAME:
           hashValue += (((SYMBOL_HN *) theResult.value)->bucket * multiplier);
           break;
             
         case INTEGER:
            hashValue += (((INTEGER_HN *) theResult.value)->bucket * multiplier);
            break;
             
         case FLOAT:
           hashValue += (((FLOAT_HN *) theResult.value)->bucket * multiplier);
           break;
        }

      /*==============================================*/
      /* Move to the next expression to be evaluated. */
      /*==============================================*/

      hashExpr = hashExpr->nextArg;
	  multiplier = multiplier * 509;
	 }

   /*=======================================*/
   /* Restore some of the global variables. */
   /*=======================================*/

   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
   EngineData(theEnv)->GlobalJoin = oldJoin;

   /*=================================================*/
   /* Return the result of evaluating the expression. */
   /*=================================================*/

   return(hashValue);
  }

/*******************************************************************/
/* PPDrive: Handles the merging of an alpha memory partial match   */
/*   with a beta memory partial match for a join that has positive */
/*   LHS entry and positive RHS entry. The partial matches being   */
/*   merged have previously been checked to determine that they    */
/*   satisify the constraints for the join. Once merged, the new   */
/*   partial match is sent to each child join of the join from     */
/*   which the merge took place.                                   */
/*******************************************************************/
globle void PPDrive(
  void *theEnv,
  struct partialMatch *lhsBinds,
  struct partialMatch *rhsBinds,
  struct joinNode *join,
  int threadID)
  {
   struct partialMatch *linker;
   struct joinLink *listOfJoins;
   unsigned long hashValue;
   
#if SLOW
   SlowDown();//add by xuchao
#endif

   /*================================================*/
   /* Send the new partial match to all child joins. */
   /*================================================*/

   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;

   /*===============================================================*/ 
   /* In the current implementation, all children of this join must */
   /* be entered from the same side (either all left or all right). */
   /*===============================================================*/
   
   while (listOfJoins != NULL)
     {
      /*==================================================*/
      /* Merge the alpha and beta memory partial matches. */
      /*==================================================*/

      linker = MergePartialMatches(theEnv,lhsBinds,rhsBinds);

      /*================================================*/
      /* Determine the hash value of the partial match. */
      /*================================================*/

      if (listOfJoins->enterDirection == LHS)
        {
         if (listOfJoins->join->leftHash != NULL)
           { hashValue = BetaMemoryHashValue(theEnv,listOfJoins->join->leftHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
      else
        { 
         if (listOfJoins->join->rightHash != NULL)
           { hashValue = BetaMemoryHashValue(theEnv,listOfJoins->join->rightHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
     
      /*=======================================================*/
      /* Add the partial match to the beta memory of the join. */
      /*=======================================================*/

#if !THREAD
	  //remove by xuchao
      UpdateBetaPMLinks(theEnv,linker,lhsBinds,rhsBinds,listOfJoins->join,hashValue,listOfJoins->enterDirection);
	 
#endif    

      if (listOfJoins->enterDirection == LHS)
        { 
#if !THREAD
		  NetworkAssertLeft(theEnv,linker,listOfJoins->join); //modify by xuchao
#else if
		  AddOneActiveNode(theEnv, linker, lhsBinds,rhsBinds,listOfJoins->join, hashValue,LHS,threadID); // add by xuchao
#endif
		}
      else
        { 
#if !THREAD
		  NetworkAssertRight(theEnv,linker,listOfJoins->join);  //modify by xuchao
#else if
		  AddOneActiveNode(theEnv, linker, lhsBinds,rhsBinds,listOfJoins->join, hashValue,RHS,threadID); // add by xuchao
#endif
	    }
      
      listOfJoins = listOfJoins->next;
     }

   return;
  }

/***********************************************************************/
/* EPMDrive: Drives an empty partial match to the next level of joins. */
/*   An empty partial match is usually associated with a negated CE    */
/*   that is the first CE of a rule.                                   */
/***********************************************************************/
globle void EPMDrive(
  void *theEnv,
  struct partialMatch *parent,
  struct joinNode *join)
  {
   struct partialMatch *linker;
   struct joinLink *listOfJoins;
   
   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;
         
   while (listOfJoins != NULL)
     {
      linker = CreateEmptyPartialMatch(theEnv);
         
      UpdateBetaPMLinks(theEnv,linker,parent,NULL,listOfJoins->join,0,listOfJoins->enterDirection); 

      if (listOfJoins->enterDirection == LHS)
        { 
#if !THREAD
		  NetworkAssertLeft(theEnv,linker,listOfJoins->join); //modify by xuchao
#else if
		  //AddOneActiveNode(theEnv, linker, listOfJoins->join, LHS); // add by xuchao
		  AddOneActiveNode(theEnv, linker, parent,NULL,listOfJoins->join, 0,LHS,0); // add by xuchao
#endif
	    }
      else
        { 
#if !THREAD
		  NetworkAssertRight(theEnv,linker,listOfJoins->join); //modify by xuchao
#else if
		  //AddOneActiveNode(theEnv, linker, listOfJoins->join, RHS); 
		  AddOneActiveNode(theEnv, linker, parent,NULL,listOfJoins->join, 0,RHS,0); // add by xuchao
#endif
	    }
        
      listOfJoins = listOfJoins->next;
     }
  }

/***************************************************************/
/* EmptyDrive: Handles the entry of a alpha memory partial     */
/*   match from the RHS of a join that is the first join of    */
/*   a rule (i.e. a join that cannot be entered from the LHS). */
/***************************************************************/
void EmptyDrive(
  void *theEnv,
  struct joinNode *join,
  struct partialMatch *rhsBinds,
  int threadID)
  {
   struct partialMatch *linker, *existsParent = NULL, *notParent;
   struct joinLink *listOfJoins;
   int joinExpr;
   unsigned long hashValue;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;
   
   /*======================================================*/
   /* Determine if the alpha memory partial match satifies */
   /* the join expression. If it doesn't then no further   */
   /* action is taken.                                     */
   /*======================================================*/

   join->memoryCompares++;

   if (join->networkTest != NULL)
     {

#if DEVELOPER
      if (join->networkTest)
        { EngineData(theEnv)->rightToLeftComparisons++; }
#endif
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = NULL;
      EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
      EngineData(theEnv)->GlobalJoin = join;

      joinExpr = EvaluateJoinExpression(theEnv,join->networkTest,join);
      EvaluationData(theEnv)->EvaluationError = FALSE;

      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;

      if (joinExpr == FALSE) return;
     }

   /*========================================================*/
   /* Handle a negated first pattern or join from the right. */
   /*========================================================*/

   if (join->patternIsNegated || (join->joinFromTheRight && (! join->patternIsExists))) /* reorder to remove patternIsExists test */
     {
      notParent = join->leftMemory->beta[0];
      if (notParent->marker != NULL)
        { return; }
        
      AddBlockedLink(notParent,rhsBinds);
      
      if (notParent->children != NULL)
        { PosEntryRetractBeta(theEnv,notParent,notParent->children); }
      /*
      if (notParent->dependents != NULL) 
		{ RemoveLogicalSupport(theEnv,notParent); } 
        */
              
      return;
     }


   /*=====================================================*/
   /* For exists CEs used as the first pattern of a rule, */
   /* a special partial match in the left memory of the   */
   /* join is used to track the RHS partial match         */
   /* satisfying the CE.                                  */
   /*=====================================================*/
  /* TBD reorder */
   if (join->patternIsExists)
     {
      existsParent = join->leftMemory->beta[0];
      if (existsParent->marker != NULL)
        { return; }
      AddBlockedLink(existsParent,rhsBinds);
     }

   /*============================================*/
   /* Send the partial match to all child joins. */
   /*============================================*/

   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;
 
   while (listOfJoins != NULL)
     {
      /*===================================================================*/
      /* An exists CE as the first pattern of a rule can generate at most  */
      /* one partial match, so if there's already a partial match in the   */
      /* beta memory nothing further needs to be done. Since there are no  */
      /* variable bindings which child joins can use for indexing, the     */
      /* partial matches will always be stored in the bucket with index 0. */
      /* Although an exists is associated with a specific fact/instance    */
      /* (through its rightParent link) that allows it to be satisfied,    */
      /* the bindings in the partial match will be empty for this CE.      */
      /*===================================================================*/

      if (join->patternIsExists)
        { linker = CreateEmptyPartialMatch(theEnv); }

      /*=============================================================*/
      /* Othewise just copy the partial match from the alpha memory. */
      /*=============================================================*/

      else
        { linker = CopyPartialMatch(theEnv,rhsBinds); }
     
      /*================================================*/
      /* Determine the hash value of the partial match. */
      /*================================================*/

      if (listOfJoins->enterDirection == LHS)
        {
         if (listOfJoins->join->leftHash != NULL)
           { hashValue = BetaMemoryHashValue(theEnv,listOfJoins->join->leftHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
      else
        { 
         if (listOfJoins->join->rightHash != NULL)
           { hashValue = BetaMemoryHashValue(theEnv,listOfJoins->join->rightHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
     
      /*=======================================================*/
      /* Add the partial match to the beta memory of the join. */
      /*=======================================================*/

#if !THREAD
	  //remove by xuchao
      if (join->patternIsExists)
        { UpdateBetaPMLinks(theEnv,linker,existsParent,NULL,listOfJoins->join,hashValue,listOfJoins->enterDirection); }
      else
        { UpdateBetaPMLinks(theEnv,linker,NULL,rhsBinds,listOfJoins->join,hashValue,listOfJoins->enterDirection); }
#endif
      if (listOfJoins->enterDirection == LHS)
        { 
#if !THREAD
		  NetworkAssertLeft(theEnv,linker,listOfJoins->join);  //modify by xuchao
		  
#else if
		  //AddOneActiveNode(theEnv, linker, listOfJoins->join, LHS); // add by xuchao
		  if (join->patternIsExists)
		  {AddOneActiveNode(theEnv, linker, existsParent, NULL, listOfJoins->join, hashValue, LHS,threadID);}
		  else
		  {AddOneActiveNode(theEnv, linker, NULL, rhsBinds, listOfJoins->join, hashValue, LHS,threadID);}
#endif		  
	    }
      else
        {
#if !THREAD
		  NetworkAssertRight(theEnv,linker,listOfJoins->join); //modify by xuchao 
#else if
		  //AddOneActiveNode(theEnv, linker, listOfJoins->join, RHS); // add by xuchao
		  if (join->patternIsExists)
		  {AddOneActiveNode(theEnv, linker, existsParent, NULL, listOfJoins->join, hashValue, RHS,threadID);}
		  else
		  {AddOneActiveNode(theEnv, linker, NULL, rhsBinds, listOfJoins->join, hashValue, RHS,threadID);}
#endif
	    }
        
      listOfJoins = listOfJoins->next;
     }
  }
  
/********************************************************************/
/* JoinNetErrorMessage: Prints an informational message indicating  */
/*   which join of a rule generated an error when a join expression */
/*   was being evaluated.                                           */
/********************************************************************/
static void JoinNetErrorMessage(
  void *theEnv,
  struct joinNode *joinPtr)
  {
   PrintErrorID(theEnv,"DRIVE",1,TRUE);
   EnvPrintRouter(theEnv,WERROR,"This error occurred in the join network\n");

   EnvPrintRouter(theEnv,WERROR,"   Problem resides in associated join\n"); /* TBD generate test case for join with JFTR */
/*
   sprintf(buffer,"   Problem resides in join #%d in rule(s):\n",joinPtr->depth);
   EnvPrintRouter(theEnv,WERROR,buffer);
*/
   TraceErrorToRule(theEnv,joinPtr,"      ");
   EnvPrintRouter(theEnv,WERROR,"\n");
  }

#endif /* DEFRULE_CONSTRUCT */
