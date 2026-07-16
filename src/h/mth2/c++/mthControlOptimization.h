

#ifndef ___mthControlOptimization_cpp_H___
#define ___mthControlOptimization_cpp_H___

#include 	"ts2/c++/stdObject.h"
#include	"mth2/c++/mVector.h"
#include	"mth2/c++/mQuaternion.h"
#include	"ts2/c++/stdArray.h"
#include	"ts2/c++/sPtr.h"

template <class __TYPE>
class mthCOcontrol_T {
public:
	INTEGER64 tim;
	mVector<__TYPE> force;
	__TYPE		yaw;
};

template <class __TYPE>
class mthCOsensor_T {
public:
	INTEGER64 tim;
	mVector<__TYPE> 	position;
	mVector<__TYPE>		a;
	mVector<__TYPE>		v;

	mQuaternion<__TYPE> 	attitude;
	mQuaternion<__TYPE>	yaw;
	__TYPE			yaw_scalar;
	__TYPE			yaw_v;
};

#define FORCE_BIT_X		1
#define FORCE_BIT_Y		2
#define FORCE_BIT_Z		4


template <class __TYPE>
class mthControlOptimization : public stdObject {
public:
	mthControlOptimization(int len) {
		control = thNEW(stdArray<mthCOcontrol_T<__TYPE> >,(2*len));
		sensor =  thNEW(stdArray<mthCOsensor_T<__TYPE> >,(len));
		force_A = mVector<__TYPE>(1,1,1);
		force_B = mVector<__TYPE>(0,0,0);
		yaw_A = 1;
		v_wind = mVector<__TYPE>(0,0,0);
		resist = 0.01;
		jitter = 0;
		correlation = 1;
	}
	mthControlOptimization(mthControlOptimization<__TYPE> * inp,int len=0) {
	int len1 = inp->control->length();
	int len2 = inp->sensor->length();
		if ( len == 0 )
			len = len2;
		if ( len > 0 ) {
		int i,c;
			control = thNEW(stdArray<mthCOcontrol_T<__TYPE> >,(2*len));
			sensor = thNEW(stdArray<mthCOsensor_T<__TYPE> >,(len));
			if ( len > len2 )
				len = len2;
			for ( i = 0 ; i < len ; i ++ )
				sensor->ary[i] = *inp->get_sens(i-len);
			for ( i = 0 ; i < 2*len ; i ++ )
				control->ary[i] = *inp->get_ctrl(i-2*len);
			if ( len > inp->count_sensor )
				count_sensor = inp->count_sensor;
			else	count_sensor = len;
			if ( 2*len > inp->count_control )
				count_control = inp->count_control;
			else	count_control = 2*len;
		}
		else {
			control = inp->control;
			sensor = inp->sensor;
			control_head = inp->control_head;
			sensor_head = inp->sensor_head;
		}
		copyin(inp,1);
	}
	~mthControlOptimization() {
	}
	int length() {
		return control->length();
	}
	mthCOcontrol_T<__TYPE> * get_ctrl(int pos);
	mthCOsensor_T<__TYPE> * get_sens(int pos);
	mthCOsensor_T<__TYPE> get_sens_by_time(INTEGER64 tim);
	mthCOcontrol_T<__TYPE> get_ctrl_by_time(INTEGER64 tim);
	void set(
		INTEGER64 tim,
		mVector<__TYPE> position,
		mQuaternion<__TYPE> attitude);
	void set(
		INTEGER64 tim,
		mVector<__TYPE> force);
	void copyin(mthControlOptimization<__TYPE> * inp,int flag=0) {
		eForce = inp->eForce;
		eYaw = inp->eYaw;
		eResist = inp->eResist;

		if ( flag || (eForce & FORCE_BIT_X) ) {
			force_A.x = inp->force_A.x;
			force_B.x = inp->force_B.x;
		}
		if ( flag || (eForce & FORCE_BIT_Y) ) {
			force_A.y = inp->force_A.y;
			force_B.y = inp->force_B.y;
		}
		if ( flag || (eForce & FORCE_BIT_Z) ) {
			force_A.z = inp->force_A.z;
			force_B.z = inp->force_B.z;
		}
		if ( flag || eYaw )
			yaw_A  = inp->yaw_A;
		if ( flag || eResist )
			resist = inp->resist;
		v_wind = inp->v_wind;
		jitter = inp->jitter;
	}
	INTEGER64 max_jitter() {
		if ( count_sensor == 0 || count_control == 0 )
			return -1;
		return get_sens(-1)->tim - get_ctrl(-count_sensor)->tim;
	}

	int adoptForce(__TYPE * est_p=0);
	int adoptWind(__TYPE * est_p=0);
	int adoptJitter(__TYPE * est_p=0);

	mVector<__TYPE>		force_A;
	mVector<__TYPE>		force_B;
	__TYPE			yaw_A;
	mVector<__TYPE>		v_wind;
	__TYPE			resist;
	INTEGER64		jitter;
	INTEGER64		min_jitter;

	unsigned		eForce:3;
	unsigned		eYaw:1;
	unsigned		eResist:1;

	__TYPE			correlation;
	int			count_sensor;
	int			count_control;
protected:
	int			control_head;
	int			sensor_head;
	sPtr<stdArray<mthCOcontrol_T<__TYPE> > > control;
	sPtr<stdArray<mthCOsensor_T<__TYPE> > > sensor;
};


#endif

