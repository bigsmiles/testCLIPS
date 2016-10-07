/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*                                                     */
/*                                                     */
/*                      add by xuchao                  */
/*******************************************************/
#ifndef _H_move
#define _H_move



#include "network.h"

#ifdef LOCALE
#undef LOCALE
#endif

#ifdef _MOVE_SOURCE_
#define LOCALE
#else
#define LOCALE extern
#endif

LOCALE void					  MoveOnJoinNetwork(void*);
LOCALE void					  AddOneActiveNode(void*, struct partialMatch*, struct partialMatch*,struct partialMatch*,struct joinNode*,unsigned long, char,int);
LOCALE void					  AddNodeFromAlpha(void*, struct joinNode*,unsigned long,struct multifieldMarker*,struct fact*,struct patternNodeHeader*);
LOCALE double						  SlowDown();

unsigned int __stdcall		  MoveOnJoinNetworkThread(void*);

#endif