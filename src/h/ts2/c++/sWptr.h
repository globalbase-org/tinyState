

#ifndef ___sWptr_cpp_H___
#define ___sWptr_cpp_H___

#include	"ts2/c++/sPtr.h"


/**
 * @brief 非所有の弱参照ポインタ (参照カウントを増やさない)。/ Non-owning weak reference (does not increment reference count).
 * @details
 * `sPtr<T>` の代わりに循環参照を防ぐために使う。
 * ただし `stdObject` の GC とは連携しないため、指す先が破棄された後にアクセスすると
 * ダングリングポインタになる点に注意。通常は寿命が保証されているメンバ変数の参照に使う。
 * / Use instead of `sPtr<T>` to break reference cycles.
 * Does NOT interact with the GC — accessing after the target is destroyed is undefined behavior.
 * Typically used for back-references with guaranteed lifetime.
 * @tparam __TYPE 指すオブジェクトの型。/ Pointed-to type.
 */
template<class __TYPE>
class sWptr : public sObject {
public:
	sWptr() {
		ptr = 0;
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sWptr(const sWptr<__TYPE2>& inp) {
	  	ptr = inp.ptr;
	}

	sWptr(const sWptr<__TYPE>& inp) {
		ptr = inp.ptr;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sWptr(const sPtr<__TYPE2>& inp) {
	  	ptr = inp.__get();
	}

	sWptr(const sPtr<__TYPE>& inp) {
		ptr = inp.__get();
	}

	~sWptr() {
		ptr = 0;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sWptr& operator=(sWptr<__TYPE2>& inp) {
		ptr = inp.ptr;
		return *this;
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sWptr& operator=(sPtr<__TYPE2>& inp) {
		ptr = inp.__get();
		return *this;
	}

	sWptr& operator=(sPtr<__TYPE> inp) {
		ptr = inp.__get();
		return *this;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE2, __TYPE>::value
	operator sPtr<__TYPE2>() {
		return sPtr<__TYPE2>(ptr);
	}
/*
	operator bool() const {
		return (ptr);
	}
*/
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value ||
	  				 std::is_base_of<__TYPE2, __TYPE>::value
	bool operator==(sWptr<__TYPE2> inp) const {
	  	return (ptr == inp.ptr);
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value ||
	  				 std::is_base_of<__TYPE2, __TYPE>::value
	bool operator!=(sWptr<__TYPE2> inp) const {
	  	return (ptr != inp.ptr);
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value ||
	  				 std::is_base_of<__TYPE2, __TYPE>::value
	bool operator==(sPtr<__TYPE2> inp) const {
	  	return (ptr == inp.__get());
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value ||
	  				 std::is_base_of<__TYPE2, __TYPE>::value
	bool operator!=(sPtr<__TYPE2> inp) const {
	  	return (ptr != inp.__get());
	}

	bool operator!=(sPtr_NULL inp) const {
	  	return (ptr != 0);
	}
	bool operator==(sPtr_NULL inp) const {
	  	return (ptr == 0);
	}

	bool is_null() const {
		if ( ptr == 0 )
			return 1;
		return 0;
	}
	bool is_notNull() const {
		if ( ptr == 0 )
			return 0;
		return 1;
	}
	/* 旧名 (2026-07-13 deprecated)。新規コードは is_null()/is_notNull() か if(p) を使う。 */
	[[deprecated("use is_null()")]]    bool is_clear()   const { return is_null(); }
	[[deprecated("use is_notNull()")]] bool is_unclear() const { return is_notNull(); }
	explicit operator bool() const {
		return ( ptr != 0 );
	}
	__TYPE * operator->() const {
		return ptr;
	}

	void clear() {
		ptr = 0;
	}

	__TYPE * __get() const {
		return ptr;
	}
	template<class __TYPE2>
	static sWptr d_cast(sWptr<__TYPE2> inp) {
	  	return sWptr(dynamic_cast<__TYPE*>(inp.ptr));
	}
protected:

	__TYPE *	ptr;
};



#endif
