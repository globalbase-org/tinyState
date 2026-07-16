

#ifndef ___sHalfArray_cpp_H___
#define ___sHalfArray_cpp_H___

template<class __TYPE>
class sHalfArray : public sObject {
public:
	sHalfArray(__TYPE * ary,int len) {
		this->ary = ary;
		this->len = len;
		effectiveLength = 0;
	}
	virtual int cmp(__TYPE &a,__TYPE & b) {
		return 0;
	}
	virtual void print(const char * msg,int a,int b) {
	}
	int ins() {
		effectiveLength ++;
		if ( effectiveLength == 1 )
			return 0;
		if ( effectiveLength > len ) {
			effectiveLength = len;
			return -1;
		}
	int i;
		i = effectiveLength-1;
		for ( ; i >= 1 ; ) {
		int j = (i-1)/2;
		print("before cmp",j,i);
		if ( cmp(ary[j],ary[i]) <= 0 )
				break;
			swap(j,i);
			print("after swap",j,i);
			i = j;
		}
		return 0;
	}
	void allIns() {
		for ( ; ins() >= 0 ; );
	}
	int del(int ptr) {
		if ( ptr < 0 )
			return -1;
		if ( ptr >= effectiveLength )
			return -1;
		if ( ptr == effectiveLength-1 ) {
			effectiveLength --;
			return 0;
		}
		ary[ptr] = ary[effectiveLength-1];
		effectiveLength --;		
		for  ( ; ; ) {
		int up1 = ptr*2+1;
		int up2 = ptr*2+2;
			if ( up1 >= effectiveLength )
				return 0;
			if ( up2 >= effectiveLength ) {
				if ( cmp(ary[ptr],ary[up1]) > 0 ) {
					swap(ptr,up1);
					ptr = up1;
				}
				else	return 0;
			}
			else if ( cmp(ary[ptr],ary[up1]) <= 0 ) {
				if ( cmp(ary[ptr],ary[up2]) > 0 ) {
					swap(ptr,up2);
					ptr = up2;
				}
				else	return 0;
			}
			else {
				if ( cmp(ary[up1],ary[up2]) > 0 ) {
					swap(ptr,up2);
					ptr = up2;
				}
				else {
					swap(ptr,up1);
					ptr = up1;
				}
			}
		}
	}
	int length() {
		return effectiveLength;
	}
protected:
	__TYPE * ary;
	int len;
	int effectiveLength;

	void swap(int a,int b) {
	__TYPE tmp;
		tmp = ary[a];
		ary[a] = ary[b];
		ary[b] = tmp;
	}
};

#endif
