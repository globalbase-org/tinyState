#ifndef ___mRange_cpp_H___
#define ___mRange_cpp_H___

#include	"mth2/c++/mObject.h"

class mRange : public mObject {
public:
	mRange(int x);
	mRange(double min,double max);
#define c1z_gt_c2	0	// c1*z >= c2
#define c1z_lt_c2	1	// c1*z <= c2
#define c1_gt_c2z	2	// c1 >= c2*z
#define c1_lt_c2z	3	// c1 <= c2*z
	mRange(double c1,double c2,int p);
	mRange operator|(mRange) const;
	mRange operator&(mRange) const;
	void print();

	double	min;
	double	max;
};

#endif
