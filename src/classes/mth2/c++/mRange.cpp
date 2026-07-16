

#include	"mth2/c++/mRange.h"

mRange::mRange(double min,double max) {
	this->min = min;
	this->max = max;
}

mRange::mRange(int x) {
	if ( x == 0 ) {
		max = -std::numeric_limits<float>::infinity();
		min = std::numeric_limits<float>::infinity();
	}
	else {
		max = std::numeric_limits<float>::infinity();
		min = -std::numeric_limits<float>::infinity();
	}
}


mRange::mRange(double c1,double c2,int p) {
double tmp;

	switch ( p ) {
	case c1_gt_c2z:
		p = c1z_lt_c2;
		tmp = c1;
		c1 = c2;
		c2 = tmp;
		break;
	case c1_lt_c2z:
		p = c1z_gt_c2;
		tmp = c1;
		c1 = c2;
		c2 = tmp;
		break;
	}
	if ( p == c1z_gt_c2 ) {
		if ( c1 == 0 ) {
			if ( c2 > 0 ) {
				max = -std::numeric_limits<float>::infinity();
				min = std::numeric_limits<float>::infinity();
			}
			else {
				max = std::numeric_limits<float>::infinity();
				min = -std::numeric_limits<float>::infinity();
			}
		}
		else if ( c1 > 0 ) {
			min = c2/c1;
			max = std::numeric_limits<float>::infinity();
		}
		else {
			max = c2/c1;
			min = -std::numeric_limits<float>::infinity();
		}
	}
	else {
		if ( c1 == 0 ) {
			if ( c2 >= 0 ) {
				max = std::numeric_limits<float>::infinity();
				min = -std::numeric_limits<float>::infinity();
			}
			else {
				max = -std::numeric_limits<float>::infinity();
				min = std::numeric_limits<float>::infinity();
			}
		}
		else if ( c1 > 0 ) {
			max = c2/c1;
			min = -std::numeric_limits<float>::infinity();
		}
		else if ( c1 < 0 ) {
			min = c2/c1;
			max = std::numeric_limits<float>::infinity();
		}
	}
}


mRange 
mRange::operator|(mRange r) const {
mRange ret(0);
	if ( r.min < min ) {
		ret.min = r.min;
	}
	else {
		ret.min = min;
	}
	if ( r.max < max ) {
		ret.max = max;
	}
	else {
		ret.max = r.max;
	}
	return ret;
}

mRange 
mRange::operator&(mRange r) const {
mRange ret(1);

	if ( r.min > min ) {
		ret.min = r.min;
	}
	else  {
		ret.min = min;
	}
	if ( r.max > max ) {
		ret.max = max;
	}
	else {
		ret.max = r.max;
	}
	if ( ret.max < ret.min ) {
		ret.max = -std::numeric_limits<float>::infinity();
		ret.min = std::numeric_limits<float>::infinity();
	}
	return ret;
}

void
mRange::print()
{
	::printf("[%lf:%lf]",min,max);
}
