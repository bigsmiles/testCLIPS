/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*                                                     */
/*                                                     */
/*                      add by xuchao                  */
/*******************************************************/
#ifndef _H_INPUT
#define _H_INPUT

#ifdef LOCALE
#undef LOCALE
#endif



#ifndef _H_factmngr
#include "factmngr.h"
#endif

#ifdef _INPUT_SOURCE_
#define LOCALE
#else
#define LOCALE extern
#endif


LOCALE void					  VariableFactAssert1(void*,const char*,const int,const int);
LOCALE void					  VariableFactAssert2(void*, const int);
LOCALE void					  VariableFactAssert3(void*, const int, const char*);
LOCALE void					  VariableFactAssert4(void*, const int, const char*);
LOCALE void					  VariableFactAssert5(void*, const int, const char*);
#endif