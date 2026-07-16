

#ifndef ___stdEvent_cpp_H___
#define ___stdEvent_cpp_H___

#include	"ts2/c++/tinyState.h"

class tinyState;

class stdEvent : public stdObject {
public:
	int		type;
	sPtr<tinyState> 	source;
	int		seq;
	INTEGER64	msg_int;
	void *		msg_ptr;
	sPtr<stdObject> 	msg_obj;

const char * debug;

	~stdEvent();
	stdEvent(
		int type,
		sPtr<tinyState>  source,
		INTEGER64 msg);
	stdEvent(
		int type,
		sPtr<tinyState>  source,
		void * msg);
	stdEvent(
		int type,
		sPtr<tinyState>  source,
		sPtr<stdObject>  msg);

	stdEvent(
		int		type,
		sPtr<tinyState> 	source,
		int		seq,
		INTEGER64	msg);
	stdEvent(
		int		type,
		sPtr<tinyState> 	source,
		int		seq,
		void *		msg);
	stdEvent(
		int		type,
		sPtr<tinyState> 	source,
		int		seq,
		sPtr<stdObject> 	msg);
	stdEvent(
		int		type,
		sPtr<tinyState> 	source,
		int		seq,
		INTEGER64	msg_int,
		void *		msg_ptr,
		sPtr<stdObject> 	msg_obj);

	stdEvent(sPtr<stdEvent>  ev);
};


#define TSE_INIT	0
#define TSE_INVOKE	1
#define TSE_TIMER	2
#define TSE_TIMER2	3
#define TSE_TIMER3	4
#define TSE_STATE	5
#define TSE_DESTROY	6
#define TSE_RETURN	7
#define TSE_ASSERT	8
#define TSE_REQUEST	9
#define TSE_UPDATED	10
#define TSE_UPDATED2	11
#define TSE_UPDATED3	12
#define TSE_SUBMIT	13
#define TSE_TRANSACTION	14
#define TSE_WAKEUP	15
#define TSE_SIGNAL	16
#define TSE_THREAD	17
#define TSE_PACKET	18
#define TSE_ACCESS	19
#define TSE_PRIORITY	20
#define TSE_MAX		21

#define TSE_EVM(x)	(1<<(x))

/* I/O サブイベント (TSE_ATTACH..TSE_READ_REMAIN, 上位ワード)・TSE_SUBMASK・TSE_MASK は
   旧 C 実装 (arch の ts2/c ツリー) 専用で、C++ 化以降どこからも使われないため削除
   (2026-07-15)。C++ の read_c/write_c は sException による yield 方式。イベント型は
   基本種別 (0..TSE_MAX) のみになり上位ワード/マスクは不要。
   未使用だった TSE_EMPTY_PROTO / TSE_INVALID_PROTO / TSE_CANCELED も同時に削除。 */


#endif


