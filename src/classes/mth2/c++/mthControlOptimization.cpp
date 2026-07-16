

#include	"mth2/c++/mthControlOptimization.h"
#include	"mth2/c++/mthMatrix.h"
#include	"mth2/c++/mthLeastSquares.h"

#define MIN_PITCH		0.0000001

template <class __TYPE>
mthCOcontrol_T<__TYPE> *
mthControlOptimization<__TYPE>::get_ctrl(int pos)
{
mthCOcontrol_T<__TYPE> ret;
	if ( pos < - control->length() || 0 <= pos )
		return 0;
	pos = pos + control_head;
	for ( ; pos < 0 ; pos += this->control->length() );
	return &control->ary[pos];
}

template <class __TYPE>
mthCOsensor_T<__TYPE> *
mthControlOptimization<__TYPE>::get_sens(int pos)
{
mthCOsensor_T<__TYPE> ret;
	if ( pos < - sensor->length() || 0 <= pos )
		return 0;
	pos = pos + sensor_head;
	for ( ; pos < 0 ; pos += this->sensor->length() );
	return &sensor->ary[pos];
}

template <class __TYPE>
mthCOsensor_T<__TYPE>
mthControlOptimization<__TYPE>::get_sens_by_time(INTEGER64 tim)
{
int min,max,mid;
mthCOsensor_T<__TYPE> get_t,min_t,max_t;;
__TYPE low,high,all;
	min = -sensor->length();
	max = -1;
	max_t = *get_sens(max);
	if ( max_t.tim <= tim ) {
		max_t.tim = tim;
		return max_t;
	}
	min_t = *get_sens(min);
	if ( min_t.tim >= tim ) {
		min_t.tim = tim;
		return min_t;
	}
	for ( ; min+1 < max ; ) {
		mid = (min + max)/2;	
		get_t = *get_sens(mid);
		if ( get_t.tim == tim )
			return get_t;
		if ( min_t.tim < tim ) {
			min = mid;
			min_t = get_t;
		}
		else {
			max = mid;
			max_t = get_t;
		}
	}
	low = tim - min_t.tim;
	high = max_t.tim - tim;
	all = low + high;
	get_t.tim = tim;
	get_t.position = (max_t.position*low + min_t.position*high)/all;
	get_t.v = (max_t.v*low + min_t.v*high)/all;
	get_t.a = (max_t.a*low + min_t.a*high)/all;

mVector<__TYPE>	t_max,t_min;
	t_max = max_t.attitude.torque();
	t_min = min_t.attitude.torque();
	t_max = (t_max*low + t_min*high)/all;
	get_t.attitude = mQuaternion<__TYPE>(t_max.norm(),t_max);
	get_t.yaw_scalar = get_t.attitude.q2rpy().z;
	get_t.yaw = mQuaternion<__TYPE>(get_t.yaw_scalar,
			mVector<__TYPE>(0,0,1));
	return get_t;
}

template <class __TYPE>
mthCOcontrol_T<__TYPE>
mthControlOptimization<__TYPE>::get_ctrl_by_time(INTEGER64 tim)
{
int min,max,mid;
mthCOcontrol_T<__TYPE> get_t,min_t,max_t;;
__TYPE low,high,all;
	min = -control->length();
	max = -1;
	max_t = *get_ctrl(max);
	if ( max_t.tim <= tim ) {
		max_t.tim = tim;
		return max_t;
	}
	min_t = *get_ctrl(min);
	if ( min_t.tim >= tim ) {
		min_t.tim = tim;
		return min_t;
	}
	for ( ; min+1 < max ; ) {
		mid = (min + max)/2;	
		get_t = *get_ctrl(mid);
		if ( get_t.tim == tim )
			return get_t;
		if ( min_t.tim < tim ) {
			min = mid;
			min_t = get_t;
		}
		else {
			max = mid;
			max_t = get_t;
		}
	}
	low = tim - min_t.tim;
	high = max_t.tim - tim;
	all = low + high;
	get_t.tim = tim;
	get_t.force = (max_t.force*low + min_t.force*high)/all;
	return get_t;
}

template <class __TYPE>
void 
mthControlOptimization<__TYPE>::set(
		INTEGER64 tim,
		mVector<__TYPE> position,
		mQuaternion<__TYPE> attitude)
{
mthCOsensor_T<__TYPE> * ptr;
mthCOsensor_T<__TYPE> * prev1,* prev2;
mVector<__TYPE> v,v2,a;
__TYPE dt1,dt2;
int p1,p2;
	count_sensor ++;
	if ( count_sensor > sensor->length() )
		count_sensor = sensor->length();
	sensor_head ++;
	if ( sensor_head >= sensor->length() )
		sensor_head = 0;
	ptr = get_sens(-1);
	ptr->position = position;
	ptr->attitude = attitude;
	ptr->tim = tim;
	ptr->yaw_scalar = attitude.q2rpy().z;
	ptr->yaw = mQuaternion<__TYPE>(ptr->yaw_scalar,
			mVector<__TYPE>(0,0,1));

	prev1 = get_sens(-2);
	prev2 = get_sens(-3);

	v = (position - prev1->position)/
		((tim - prev1->tim)/1000000.0);

	dt1 = (prev1->tim - prev2->tim)/1000000.0;
	dt2 = (tim - prev2->tim)/1000000.0;

mthMatrix<__TYPE> mx1,mx2;
	mx1 = mthMatrix<__TYPE>(3,3);
	mx2 = mthMatrix<__TYPE>(3,3);

	mx1.set(1,0,0);
	mx1.set(1,1,dt1);
	mx1.set(1,2,dt2);

	mx1.set(2,0,1);
	mx1.set(2,1,1);
	mx1.set(2,2,1);

	mx2.set(0,0,0);
	mx2.set(0,1,dt1*dt1);
	mx2.set(0,2,dt2*dt2);

	mx2.set(1,0,0);
	mx2.set(1,1,dt1);
	mx2.set(1,2,dt2);

	mx2.set(2,0,1);
	mx2.set(2,1,1);
	mx2.set(2,2,1);

	mx1.set(0,0,prev2->position.x);
	mx1.set(0,1,prev1->position.x);
	mx1.set(0,2,position.x);
	ptr->a.x = mx1.det()/mx2.det()*2;

	mx1.set(0,0,prev2->position.y);
	mx1.set(0,1,prev1->position.y);
	mx1.set(0,2,position.y);
	ptr->a.y = mx1.det()/mx2.det()*2;
	mx1.set(0,0,prev2->position.z);
	mx1.set(0,1,prev1->position.z);
	mx1.set(0,2,position.z);
	ptr->a.z = mx1.det()/mx2.det()*2;

	ptr->v = v;
	ptr->a = a;
	prev1->a = ptr->a;

__TYPE y;
	y = ptr->yaw_scalar - prev1->yaw_scalar;
	for ( ; y < -M_PI ; y += M_PI*2 );
	for ( ; y > M_PI ; y -= M_PI*2 );
	ptr->yaw_v = y/(ptr->tim - prev1->tim)*1000000.0;
}

template <class __TYPE>
void 
mthControlOptimization<__TYPE>::set(
		INTEGER64 tim,
		mVector<__TYPE> force)
{
mthCOcontrol_T<__TYPE> * ptr;
	count_control ++;
	if ( count_control >= control->length() )
		count_control = control->length();
	control_head ++;
	if ( control_head >= control->length() )
		control_head = 0;
	ptr = get_ctrl(-1);
	ptr->tim = tim;
	ptr->force = force;
}


template <class __TYPE>
int
mthControlOptimization<__TYPE>::adoptForce(__TYPE * est_p)
{
mthMatrix<__TYPE> lsmx,lsv;
mVector<__TYPE> resist_v;
mthLeastSquares<__TYPE> ls;
mthMatrix<__TYPE> res;
int i;

mVector<__TYPE> coForce_sq,coForce_avg,coForce;
__TYPE coYaw_sq,coYaw_avg,coYaw;
__TYPE coRV_sq,coRV_avg,coRV;
__TYPE est,est_avg;

	coForce_sq = coForce_avg = mVector<__TYPE>(0,0,0);
	coYaw_sq = coYaw_avg = coRV_sq = coRV_avg = 0;

	lsmx = mthMatrix<__TYPE>(length()*4,8);
	lsv  = mthMatrix<__TYPE>(length()*4,1);

	for ( i = -1 ; i >= -length() ; i -- ) {
	mVector<__TYPE> rv,a;
	mthCOsensor_T<__TYPE> * s_ptr;
	mthCOcontrol_T<__TYPE> ctrl;
		s_ptr = get_sens(i);
		a = s_ptr->yaw*s_ptr->a*s_ptr->yaw.t();
		ctrl = get_ctrl_by_time(s_ptr->tim - jitter);
		lsv.set(i*4,  0,s_ptr->a.x);
		lsv.set(i*4+1,0,s_ptr->a.y);
		lsv.set(i*4+2,0,s_ptr->a.z);
		lsv.set(i*4+3,0,s_ptr->yaw_v);

		rv = s_ptr->v - v_wind;
		rv = s_ptr->yaw*(rv * (-rv.norm()))*s_ptr->yaw.t();

		lsmx.set(i*4,  0,rv.x);
		lsmx.set(i*4+1,0,rv.y);
		lsmx.set(i*4+2,0,rv.z);

		lsmx.set(i*4,  1,ctrl.force.x);
		lsmx.set(i*4+1,2,ctrl.force.y);
		lsmx.set(i*4+2,3,ctrl.force.z);
		lsmx.set(i*4+3,4,ctrl.yaw);
		lsmx.set(i*4,  5,1);
		lsmx.set(i*4+1,6,1);
		lsmx.set(i*4+2,7,1);

		coForce_avg += ctrl.force;
		coForce_sq += ctrl.force.vmul(ctrl.force);
		coYaw_avg += ctrl.yaw*ctrl.yaw;
		coYaw_sq += ctrl.yaw;

		coRV_avg += rv*mVector<__TYPE>(1,1,1);
		coRV_sq += rv.norm2();
	}
	ls = mthLeastSquares<__TYPE>(lsmx,lsv);
	res = ls.result();
	if ( res.get_err() < 0 )
		return -1;
	resist = res.elm(0,0);
	force_A.x = res.elm(1,0);
	force_A.y = res.elm(2,0);
	force_A.z = res.elm(3,0);
	force_B.x = res.elm(4,0);
	force_B.y = res.elm(5,0);
	force_B.z = res.elm(6,0);
	yaw_A     = res.elm(7,0);

	est = ls.estimate(res);
	est_avg = est/length();
	coForce_sq /= length();
	coForce_avg /= length();
	coForce = coForce_sq - coForce_avg.vmul(coForce_avg);
	coYaw_sq /= length();
	coYaw_avg /= length();
	coYaw = coYaw_sq - coYaw_avg*coYaw_avg;
	coRV_sq /= length();
	coRV_avg /= length();
	coRV = coRV_sq - coRV_avg*coRV_avg;

	eForce = 0;
	if ( coForce.x && est_avg/coForce.x < correlation )
		eForce |= FORCE_BIT_X;
	if ( coForce.y && est_avg/coForce.y < correlation )
		eForce |= FORCE_BIT_Y;
	if ( coForce.z && est_avg/coForce.z < correlation )
		eForce |= FORCE_BIT_Z;
	if ( coYaw && est_avg/coYaw < correlation )
		eYaw = 1;
	else	eYaw = 0;
	if ( coRV && est_avg/coRV < correlation )
		eResist = 1;
	else	eResist = 0;


	if ( est_p )
		*est_p = est;
	return 0;
}

template <class __TYPE>
int
mthControlOptimization<__TYPE>::adoptWind(__TYPE * est_p)
{
mVector<__TYPE> wind = v_wind;
__TYPE est;
__TYPE pitch;
	if ( adoptForce(&est) < 0 )
		return -1;
	pitch = 0.01;
	for ( ; pitch > MIN_PITCH ; ) {
	__TYPE n_est;
		v_wind = wind;
		v_wind.x += pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}
		v_wind = wind;
		v_wind.x -= pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}

		v_wind = wind;
		v_wind.y += pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}
		v_wind = wind;
		v_wind.y -= pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}

		v_wind = wind;
		v_wind.z += pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}
		v_wind = wind;
		v_wind.z -= pitch;
		if ( adoptForce(&n_est) < 0 )
			return -1;
		if ( n_est < est ) {
			wind = v_wind;
			est = n_est;
			pitch *= 1.3;
			continue;
		}

		pitch *= 0.5;
	}
	if ( !(v_wind == wind) ) {
		v_wind = wind;
		adoptForce(&est);
	}
	if ( est_p )
		*est_p = est;
	return 0;
}

template <class __TYPE>
int
mthControlOptimization<__TYPE>::adoptJitter(__TYPE * est_p)
{
INTEGER64 t_jitter = jitter;
__TYPE est;
INTEGER64 pitch;
	if ( max_jitter() <= min_jitter )
		return -1;
	if ( t_jitter < min_jitter )
		t_jitter = min_jitter;
	jitter = t_jitter;
	if ( adoptWind(&est) < 0 )
		return -1;
	pitch = 5;
	for ( ; pitch ; ) {
	__TYPE n_est;
	INTEGER64 p;
		jitter = t_jitter + pitch;
		if ( jitter <= max_jitter() ) {
			if ( adoptWind(&n_est) < 0 )
				return -1;
			if ( n_est < est ) {
				t_jitter = jitter;
				est = n_est;
				p = pitch *= 1.3;
				if ( p == pitch )
					pitch = p+1;
				else	pitch = p;
				continue;
			}
		}
		jitter = t_jitter - pitch;
		if ( jitter >= min_jitter ) {
			if ( adoptWind(&n_est) < 0 )
				return -1;
			if ( n_est < est ) {
				t_jitter = jitter;
				est = n_est;
				p = pitch *= 1.3;
				if ( p == pitch )
					pitch = p+1;
				else	pitch = p;
				continue;
			}
		}
		pitch *= 0.5;
	}
	if ( jitter != t_jitter ) {
		jitter = t_jitter;
		adoptWind(&est);
	}
	if ( est_p )
		*est_p = est;
	return 0;
}


template class mthControlOptimization<float>;
template class mthControlOptimization<double>;
template class mthControlOptimization<long double>;
