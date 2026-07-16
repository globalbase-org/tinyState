

#ifndef ___mthAdopt_cpp_H___
#define ___mthAdopt_cpp_H___


template<class __TYPE>
class mthAdopt : public stdObject {
public:
	mthAdopt() {
	}
	~mthAdopt() {
	}
	__TYPE exe() {
	__TYPE result;
	int do_flag;
		result = eval(-1,&result);
		if ( result < 0 )
			return result;
		for ( ; ; ) {
		do_flag = 0;
		int pos,ret;
			for ( pos = 0 ; pos < paramCount ; pos ++ ) {
				ret = exe_one(&result,pos,1);
				if ( ret == 1 ) {
					do_flag = 1;
					continue;
				}
				if ( ret < 0 )
					return ret;
				ret = exe_one(&result,pos,-1);
				if ( ret == 1 )
					do_flag = 1;
				if ( ret < 0 )
					return ret;
			}
			if ( do_flag == 0 )
				break;
		}
		return result;
	}
protected:
	int exe_one(__TYPE * result,int pos,int sign) {
	int ret = 0;
	__TYPE pitch;

		pitch = pitchMin(pos)*1.1;
		for ( ; pitch >= pitchMin(pos) ;  ) {
		__TYPE v;
		__TYPE r,vv;
			v = get(pos);
			if ( stopCheck() )
				return -3;
			vv = v + sign*pitch;
			if ( !valueTest(pos,vv) ) {
				pitch *= 0.5;
				continue;
			}
			set(pos,vv);
			r = eval(pos,result);
			if ( r >= 0 && r < *result ) {
//::printf("exe_one %i %i - %lf %lf\n",pos,sign,*result,r);
				ret = 1;
				*result = r;
				pitch *= 1.3;
				if ( pitch > pitchMax(pos) )
					pitch = pitchMax(pos);
				continue;
			}
			back(pos);
			pitch *= 0.5;
		}
		return ret;
	}

	virtual void set(int pos,__TYPE v)  {
	}
	virtual __TYPE get(int pos) {
		return 0;
	}
	virtual void back(int pos) {
	}
	virtual int valueTest(int pos,__TYPE v) {
		return 1;
	}
	virtual __TYPE pitchMax(int pos) {
		return 0;
	}
	virtual __TYPE pitchMin(int pos) {
		return 0;
	}
	virtual __TYPE eval(int pos,__TYPE * result_p) {
		return 0;
	}
	virtual int stopCheck() {
		return 0;
	}

	int paramCount;
};
#endif
