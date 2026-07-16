

#ifndef ___sPicoState_cpp_H___
#define ___sPicoState_cpp_H___



#define	PS_STATEMENT(__name,__x,__y)	\
	{				\
	typeof(__name)*	__np = &__name;	\
	const char * traceFlag = 0;	\
		if ( __np->__recursive )\
			sObject::panic("sPicoState function cannot recursive call\n"); 	\
	__sFlag __f(__np->__recursive);	\
		PS_DEF(__state)		\
		__x			\
		switch ( __state ) {	\
		case -1:		\
		__y			\
		}			\
	}
#define PS_STATE(__x)	\
			if ( traceFlag )					\
				::printf("%s <<< %i\n",traceFlag,__state);	\
		case __x:							\
		__##__x:							\
			__state = __x;						\
			if ( traceFlag )					\
				::printf("%s     %i >>>\n",traceFlag,__state);
#define PS_RETURN(__x)	{	\
		if ( traceFlag )						\
				::printf("%s <<< %i\n",traceFlag,__state);	\
		__state = psINI;						\
		if ( traceFlag )						\
				::printf("%s     %i >>> RETURN\n",traceFlag,__state);	\
		return __x;}
#define PS_GOTO(__x)	\
		if ( traceFlag )						\
				::printf("%s <<< %i\n",traceFlag,__state); 	\
		goto __##__x;
#define PS_DEF(__x)	\
	typeof(__np->__x)& __x(__np->__x);
#define PS_PRESET	\
	int __state;		\
	int __recursive;	\

#define psINI		0
#define psDO		1
#define psFINISH	2


class __sFlag : sObject {
public:
	__sFlag(int& rec) : rp(rec) { rp = 1; }
	~__sFlag() { rp = 0; }
protected:
	int & rp;
};


#endif
