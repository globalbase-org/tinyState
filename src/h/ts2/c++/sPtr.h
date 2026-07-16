// TS2_DONT_MODIFY

#ifndef ___sPtr_cpp_H___
#define ___sPtr_cpp_H___

#include	"ts2/c++/sObject.h"
#include	"ts2/c++/stdObject.h"


template<class __TYPE>
class sWptr;

template<class __TYPE1,class __TYPE2>
class sRptr;


/** @brief thNULL のための番兵型。`sPtr` に代入すると nullptr にリセットする。/ Sentinel type for thNULL; assigning to sPtr resets it to nullptr. */
class sPtr_NULL : public sObject {
 public:
	bool is_null() const {
		return 0;
	}
	bool is_notNull() const {
		return 1;
	}
	/* 旧名 (2026-07-13 deprecated)。互換のため残置。新規コードは is_null()/is_notNull() を使う。 */
	[[deprecated("use is_null()")]]    bool is_clear()   const { return is_null(); }
	[[deprecated("use is_notNull()")]] bool is_unclear() const { return is_notNull(); }
};

#define thNULL	sPtr_NULL()
#define IS_thVALID(x)	((x).is_notNull())

/**
 * @brief 参照カウント付きスマートポインタ。tinyState フレームワークの標準ポインタ型。/ Reference-counted smart pointer — the standard pointer type in tinyState.
 * @details
 * `std::shared_ptr` 相当だが `stdObject` の `addref()`/`relref()` を直接呼ぶ。
 * `thNEW(T, (args))` で生成し、`thNULL` でリセット。`is_notNull()` で非 null チェック。
 * `d_cast<U>(p)` で `dynamic_cast` 相当のダウンキャスト。
 *
 * - `is_null()` / `is_notNull()` : null / 非 null チェック (旧 `is_clear()` / `is_unclear()` は deprecated)
 * - `if (p)` / `if (!p)` : explicit operator bool による null 判定 (bool 文脈のみ)
 * - `d_cast<U>(p)` : dynamic cast
 * - `thThis` : 自身への sPtr (`sPtr<typeof(*this)>(this)`)
 *
 * / Equivalent to `std::shared_ptr` but calls `addref()`/`relref()` directly on `stdObject`.
 * @tparam __TYPE 指すオブジェクトの型 (`stdObject` 派生)。/ Pointed-to type (must derive from `stdObject`).
 */
template<class __TYPE>
class sPtr : public sObject {
public:
	sPtr() {
		ptr = 0;
	}
	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sPtr(__TYPE2 * org,int mutex_flag=0) {
		_initial(org,mutex_flag);
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sPtr(const sPtr<__TYPE2> inp) {
	  	static_assert(std::is_base_of<__TYPE, __TYPE2>::value == true, "invalid type");
		_initial(inp.__get());
	}

	sPtr(const sPtr<__TYPE>& inp) {
		_initial(inp.__get());
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value && (!std::is_same<__TYPE,__TYPE2>::value)
	sPtr(const sWptr<__TYPE2>& inp) {
	  	static_assert(std::is_base_of<__TYPE, __TYPE2>::value == true, "invalid type");
		_initial(inp.__get());
	}

	sPtr(const sWptr<__TYPE>& inp) {
		_initial(inp.__get());
	}

	template<class __TYPE1,class __TYPE2> 
				requires std::is_base_of<__TYPE, __TYPE1>::value &&
					 std::is_base_of<__TYPE, __TYPE2>::value
	sPtr(const sRptr<__TYPE1,__TYPE2> inp) {
	  	static_assert(std::is_base_of<__TYPE, __TYPE1>::value == true, "invalid type");
	  	static_assert(std::is_base_of<__TYPE, __TYPE2>::value == true, "invalid type");
		_initial(inp.__get());
	}

	sPtr(const sPtr_NULL& inp) {
		_initial(0);
	}


	~sPtr() {
		replace(0);
	}

	sPtr operator=(sPtr_NULL inp) {
		replace(0);
		return *this;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sPtr operator=(sPtr<__TYPE2> inp) {
		replace(inp.__get());
		return *this;
	}

	sPtr operator=(sPtr inp) {
		replace(inp.__get());
		return *this;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
	sPtr operator=(sWptr<__TYPE2> inp) {
		replace(inp.__get());
		return *this;
	}

	sPtr operator=(sWptr<__TYPE> inp) {
		replace(inp.__get());
		return *this;
	}

	template<class __TYPE2> requires std::is_base_of<__TYPE2, __TYPE>::value && (!std::is_same<__TYPE,__TYPE2>::value)
	operator sWptr<__TYPE2>() const {
		return sWptr<__TYPE2>(*this);
	}

	sPtr operator=(__TYPE * inp) {
		replace(inp);
		return *this;
	}

	/* bool 文脈 (if(ptr) / if(!ptr) / while / && / ||) で使えるようにする。
	 * explicit にすることで int への暗黙変換や a==b の暗黙 bool 化といった誤用を
	 * 防ぐ (std::shared_ptr / unique_ptr / optional と同じイディオム)。 */
	explicit operator bool() const {
		return ( ptr != 0 );
	}
	template<class __TYPE2>
	bool operator==(sPtr<__TYPE2> inp) const {
	  	return (ptr == inp.__get());
	}
	template<class __TYPE2>
	bool operator!=(sPtr<__TYPE2> inp) const {
	  	return (ptr != inp.__get());
	}

	template<class __TYPE2>
	bool operator==(sWptr<__TYPE2> inp) const {
	  	return (ptr == inp.__get());
	}
	template<class __TYPE2>
	bool operator!=(sWptr<__TYPE2> inp) const {
	  	return (ptr != inp.__get());
	}

	bool operator!=(sPtr_NULL inp) const {
	  	return (ptr != 0);
	}
	bool operator==(sPtr_NULL inp) const {
	  	return (ptr == 0);
	}
/*
	bool operator==(sPtr<stdObject>  inp) const {
	  	return (ptr == inp);
	}
	bool operator!=(sPtr<stdObject>  inp) const {
	  	return (ptr != inp);
	}
*/
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
	/* 旧名 (2026-07-13 deprecated)。互換のため残置。新規コードは is_null()/is_notNull()
	 * または if(ptr)/if(!ptr) を使う。 */
	[[deprecated("use is_null()")]]    bool is_clear()   const { return is_null(); }
	[[deprecated("use is_notNull()")]] bool is_unclear() const { return is_notNull(); }
	__TYPE * operator->() const {
		return ptr;
	}

	void clear() {
		replace(0);
	}

	__TYPE * __get() const {
		return ptr;
	}
	__TYPE * sPtr__get() const {
		return ptr;
	}
	template<class __TYPE2>
	static sPtr d_cast(sPtr<__TYPE2> inp) {
	  	return sPtr(dynamic_cast<__TYPE*>(inp.__get()));
	}
	template<class __TYPE2>
	static sPtr d_cast(sWptr<__TYPE2> inp) {
	  	return sPtr(dynamic_cast<__TYPE*>(inp.__get()));
	}
	template<class __TYPE2,class __TYPE3>
	  static sPtr d_cast(sRptr<__TYPE2,__TYPE3> inp) {
	  	return sPtr(dynamic_cast<__TYPE*>(inp.__get()));
	}

protected:

	void _replace(__TYPE * inp) {
		if ( ptr == inp )
			return;
		if ( ptr )
			ptr->relref();
		ptr = inp;
		if ( ptr )
			ptr->addref();
	}
	void _initial(__TYPE * inp,int flag=0) {
		ptr = inp;
		if ( flag == 0 && ptr )
		  	ptr->addref();
	}
	void replace(__TYPE * inp) {
		_replace(inp);
	}

	__TYPE *	ptr;
};


#define thThis		(sPtr<typeof(*this)>(this))
// #define dynamicCast(__x,__y)	sPtr<__x>(dynamic_cast<__x*>(__y.__get()))
#define __CMA		,

#endif
