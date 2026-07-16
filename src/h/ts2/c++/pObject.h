
#ifndef ___pObject_cpp_H___
#define ___pObject_cpp_H___

#include	<new>

class pObject {
public:
	static void * operator new(size_t cbSize,void * ptr)
#if __cplusplus >= 201103L
			noexcept(false)
#else
			throw(std::bad_alloc)
#endif
	{
		return ptr;
 	}
	static void operator delete(void * pv)
#if __cplusplus >= 201103L
    		noexcept(true)
#else
		throw()
#endif
	{
  	}
};

#endif
