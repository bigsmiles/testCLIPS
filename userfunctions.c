   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*               CLIPS Version 6.24  04/21/06          */
   /*                                                     */
   /*                USER FUNCTIONS MODULE                */
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
/*      6.24: Created file to seperate UserFunctions and     */
/*            EnvUserFunctions from main.c.                  */
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
#include "clips.h"

void UserFunctions(void);
void EnvUserFunctions(void *);
extern void *betaEnv;
/*********************************************************/
/* UserFunctions: Informs the expert system environment  */
/*   of any user defined functions. In the default case, */
/*   there are no user defined functions. To define      */
/*   functions, either this function must be replaced by */
/*   a function with the same name within this file, or  */
/*   this function can be deleted from this file and     */
/*   included in another file.                           */
/*********************************************************/
void UserFunctions()
  {   
  }
  
/***********************************************************/
/* EnvUserFunctions: Informs the expert system environment */
/*   of any user defined functions. In the default case,   */
/*   there are no user defined functions. To define        */
/*   functions, either this function must be replaced by   */
/*   a function with the same name within this file, or    */
/*   this function can be deleted from this file and       */
/*   included in another file.                             */
/***********************************************************/
#if WIN_BTC
#pragma argsused
#endif
void EnvUserFunctions(
  void *theEnv)
  {
#if MAC_MCW || WIN_MCW || MAC_XCD
#pragma unused(theEnv)
#endif
	extern int after(void*);
	extern long long getTime();
	extern long long getFactTime();

	EnvDefineFunction2(theEnv, "after", 'i', PTIEF after, "after", "22h");
	EnvDefineFunction2(theEnv, "getTime", 'g', PTIEF getTime, "getTime", "00");
	EnvDefineFunction2(theEnv, "getFactTime", 'g', PTIEF getFactTime, "getTime", "11");
  }
long long getTime(){
	__int64 counterOfTimer;
	LARGE_INTEGER large_time;
	char times[64];
	long long time;
	QueryPerformanceCounter(&large_time);
	time = (long long)large_time.QuadPart;

	//printf("%I64d", time);
	
	return time;

}
long long getFactTime(void* theEnv){
	//theEnv = betaEnv;
	DATA_OBJECT first,arg1;
	void *address;
	long long time = 0;
	
	if (EnvArgCountCheck(theEnv, "after", EXACTLY, 1) == -1){
		return -1;
	}
	EnvRtnUnknown(theEnv, 1, &first);
	if (GetType(first) == FACT_ADDRESS){
		address = DOToPointer(first);
		EnvGetFactSlot(theEnv, address, "time", &arg1);
	}
	else {
		printf("type = %d\n", GetType(first)); return 0;
	}
	if (GetType(arg1) == INTEGER){
		//struct integerHashNode *p = ((struct integerHashNode*)arg1.value);
		//printf("arg1 : %lld\n", ((struct integerHashNode*)arg1.value)->contents);
		//time = ValueToLong(arg1.value);
		time = DOToLong(arg1);
	}
	//printf("time = %lld\n", time);
	return time;
}
int after(void* theEnv){
	//暂时不做检查
	/*DATA_OBJECT first, second;
	EnvRtnUnknown(theEnv, 1,&first);
	EnvRtnUnknown(theEnv, 2,&second);
	printf("**%d**\n", GetType(first));
	printf("%d %d\n",first,GetValue(first));
	*/
	//theEnv = betaEnv;
	DATA_OBJECT first, second;
	DATA_OBJECT arg1,arg2;
	long long time1, time2;
	void* address1, *address2;
	if (EnvArgCountCheck(theEnv, "after", EXACTLY, 2) == -1){
		return -1;
	}
	EnvRtnUnknown(theEnv, 1,&first);
	if (GetType(first) == INSTANCE_NAME)
	{

		address1 = EnvFindInstance(theEnv, NULL, DOToString(first), FALSE); 
		EnvDirectGetSlot(theEnv, address1, "endTime", &arg1);
	}
	else if (GetType(first) == INSTANCE_ADDRESS){
		address1 = DOToPointer(first);
		EnvDirectGetSlot(theEnv, address1, "endTime", &arg1);
	}
	else if (GetType(first) == FACT_ADDRESS){
		address1 = DOToPointer(first);
		EnvGetFactSlot(theEnv, address1, "endTime",&arg1);
	}
	else{
		printf("arg should be INSTANCE_NAME,INSTANCE_ADDRESS or FACT_ADDRESS");
	}
	EnvRtnUnknown(theEnv, 2,&second);
	if (GetType(second) == INSTANCE_NAME)
	{
		address2 = EnvFindInstance(theEnv, NULL, DOToString(second), FALSE);
		EnvDirectGetSlot(theEnv, address2, "endTime", &arg2);
	}
	else if (GetType(second) == INSTANCE_ADDRESS){
		address2 = DOToPointer(second);
		EnvDirectGetSlot(theEnv, address2, "endTime", &arg2);
	}
	else if (GetType(second) == FACT_ADDRESS){
		address2 = DOToPointer(second);
		EnvGetFactSlot(theEnv, address2, "endTime",&arg2);
	}
	else{
		printf("arg should be INSTANCE_NAME or FACT_ADDRESS");
	}
	//void* address1 = EnvFindInstance(theEnv, NULL, first, FALSE);
	//void* address2 = EnvFindInstance(theEnv, NULL, second, FALSE);
	//EnvDirectGetSlot(theEnv, address1, "endTime", &arg1);
	//EnvDirectGetSlot(theEnv, address2, "startTime", &arg2);
	if (GetType(arg1) == INTEGER){
		//printf("arg1 is integer: %d\n", DOToLong(arg1));
		time1 = DOToLong(arg1);
	}
	else{
		printf("arg1 type should be integer: %d !\n",GetType(arg1));
	}
	if (GetType(arg2) == INTEGER){
		//printf("arg2 is integer : %d\n", DOToLong(arg2));
		time2 = DOToLong(arg2);
	}
	else{
		printf("arg2 type should be integer: %d !\n", GetType(arg2));
	}

	if (time1 > time2)return 1;
	else return -1;
}

