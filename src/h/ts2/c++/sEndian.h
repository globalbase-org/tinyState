

#ifndef ___sEndian_cpp_H___
#define ___sEndian_cpp_H___

#include		"ts2/c++/sObject.h"

#define ENDIAN_BIG		1
#define ENDIAN_LITTLE	0

#pragma pack(1)

class sEndian : public sObject {
public:
	sEndian() {
		d.d16 = 0x100;
	}
	operator int() {
		return d.d8[0];
	}
	int operator=(int inp) {
		sObject::panic("not write this variable");
		return 0;
	}

	static void swap(int8_t * buf,int len);
	static INTEGER64 conv(int8_t * buf,int len,int endian);
	static void conv(int8_t * buf,INTEGER64 inp,int len,int endian);

	template<typename __TYPE>
	static __TYPE swap(__TYPE inp) {
		swap((int8_t*)&inp,sizeof(inp));
		return inp;
	}

	template<typename __TYPE>
	static __TYPE conv(__TYPE inp,int endian) {
		if ( cpu == endian )
			return inp;
		return swap(inp);
	}

	template<typename __TYPE>
	static void apply(__TYPE& inp,int endian) {
		if ( cpu == endian )
			return;
		swap(inp);
	}

	static sEndian cpu;
protected:
	union {
		uint8_t		d8[2];
		uint16_t	d16;
	} d;
};

#pragma pack()

#endif

