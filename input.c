/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*                                                     */
/*                                                     */
/*                      add by xuchao                  */
/*******************************************************/
#define _INPUT_SOURCE_

#include <stdio.h>
#define _STDIO_INCLUDED_
#include <string.h>

#include "setup.h"

#include "input.h"

globle void VariableFactAssert1(void *theEnv, const char* name,const int x, const int y) //name = "data","bar","foo","room"
{
	char tmpBuffer[100];
	sprintf(tmpBuffer, "(%s %d %d)", name, x, y);
	EnvAssertString(theEnv, tmpBuffer);
}
globle void VariableFactAssert2(void* theEnv, const int id) //name = "id"
{
	char tmpBuffer[100];
	sprintf(tmpBuffer, "(id %d)", id);
	EnvAssertString(theEnv, tmpBuffer);
}
globle void VariableFactAssert3(void* theEnv, const int id, const char* name) //name = "student"
{
	char tmpBuffer[100];
	sprintf(tmpBuffer, "(student (id %d) (name \"%s\"))", id, name);
	EnvAssertString(theEnv, tmpBuffer);
}
globle void VariableFactAssert4(void* theEnv, const int id, const char* name) //name = "tom"
{
	char tmpBuffer[100];
	sprintf(tmpBuffer, "(tom id %d name %s)", id, name);
	EnvAssertString(theEnv, tmpBuffer);
} 
globle void VariableFactAssert5(void* theEnv, const int id, const char* name) //name = "tim"
{
	char tmpBuffer[100];
	sprintf(tmpBuffer, "(tim id %d name %s)", id, name);
	EnvAssertString(theEnv, tmpBuffer);
}
