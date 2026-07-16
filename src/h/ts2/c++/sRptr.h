

#ifndef ___sRptr_cpp_H___
#define ___sRptr_cpp_H___

#include	"ts2/c++/sObject.h"
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"


template<class __TYPE>
class sWptr;
template<class __TYPE>
class sPtr;



/**
 * @brief 元の `sPtr<FROM>` を実際に保持しながら `__TYPE*` として見せる参照型ポインタ。/ Reference pointer that holds a `sPtr<FROM>` but exposes it as `__TYPE*`.
 * @details
 * tinyState では `ifp` (public interface pointer) と実装クラスの sPtr が別なため、
 * 一方を変更すると他方も追随させたい場合に使う。
 * `sRptr<DerivedIface, BaseImpl> rp(myBaseSptr)` のように宣言し、
 * `rp = newValue` で元の `sPtr<FROM>` を更新しながら `__TYPE*` として使える。
 * / Used when the tinyState interface pointer (`ifp`) and implementation `sPtr` need to stay in sync.
 * Assignment to `sRptr` updates the underlying `sPtr<FROM>`.
 * @tparam __TYPE   露出する型 (`FROM` の派生)。/ Exposed type (derived from `__FROM`).
 * @tparam __FROM   実際に保持している `sPtr` の型。/ Actual held `sPtr` type.
 */
template<class __TYPE,class __FROM>
class sRptr : public sObject {
public:
	sRptr(sPtr<__FROM>& inp) 
		: ptr(inp)
	{
	}

	template<class __TYPE1>
	sRptr(sRptr<__TYPE1,__FROM>& inp) 
		: ptr(inp.ptr)
	{
	}

	~sRptr() {
	}

	sPtr<__TYPE> operator=(sPtr_NULL inp) {
		
		return sPtr<__TYPE>();
	}

	template<class __TYPE2>
	sPtr<__TYPE> operator=(sRptr<__TYPE2,__FROM> inp) {
		ptr = inp.__get();
		return ptr;
	}

	sPtr<__TYPE> operator=(sRptr inp) {
		ptr = inp.__get();
		return ptr;
	}

	template<class __TYPE2>
	sPtr<__TYPE> operator=(sPtr<__TYPE2> inp) {
		ptr = inp;
		return ptr;
	}

	sPtr<__TYPE> operator=(sPtr<__TYPE> inp) {
		ptr = inp;
		return ptr;
	}

	template<class __TYPE2>
	sPtr<__TYPE> operator=(sWptr<__TYPE2> inp) {
		ptr = inp;
		return ptr;
	}

	sPtr<__TYPE> operator=(sWptr<__TYPE> inp) {
		ptr = inp;
		return ptr;
	}

	sPtr<__TYPE> operator=(__TYPE * inp) {
		ptr = inp;
		return ptr;
	}

	template<class __TYPE2,class __TYPE3>
	bool operator==(sRptr<__TYPE2,__TYPE3> inp) const {
	  	return (ptr == inp.__get());
	}
	template<class __TYPE2,class __TYPE3>
	bool operator!=(sRptr<__TYPE2,__TYPE3> inp) const {
	  	return (ptr != inp.__get());
	}

	template<class __TYPE2>
	bool operator==(sWptr<__TYPE2> inp) const {
	  	return (ptr == inp);
	}
	template<class __TYPE2>
	bool operator!=(sWptr<__TYPE2> inp) const {
	  	return (ptr != inp);
	}

	template<class __TYPE2>
	bool operator==(sPtr<__TYPE2> inp) const {
	  	return (ptr == inp);
	}
	template<class __TYPE2>
	bool operator!=(sPtr<__TYPE2> inp) const {
	  	return (ptr != inp);
	}

	bool operator!=(sPtr_NULL inp) const {
	  	return (ptr.is_notNull());
	}
	bool operator==(sPtr_NULL inp) const {
	  	return (ptr.is_null());
	}
	operator sPtr<__TYPE>() const {
		return sPtr<__TYPE>::d_cast(ptr);
	}

	bool is_null() const {
		return  ptr.is_null();
	}
	bool is_notNull() const {
		return ptr.is_notNull();
	}
	/* 旧名 (2026-07-13 deprecated)。新規コードは is_null()/is_notNull() か if(p) を使う。 */
	[[deprecated("use is_null()")]]    bool is_clear()   const { return is_null(); }
	[[deprecated("use is_notNull()")]] bool is_unclear() const { return is_notNull(); }
	explicit operator bool() const {
		return ptr.is_notNull();
	}
	__TYPE * operator->() const {
		return dynamic_cast<__TYPE*>(ptr.__get());
	}

	void clear() {
		ptr.clear();
	}

	__TYPE * __get() const {
		return dynamic_cast<__TYPE*>(ptr.__get());
	}
protected:
	sPtr<__FROM>&	ptr;
};


#endif
