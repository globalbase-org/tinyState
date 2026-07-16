


#include	"mth2/c++/mEllipsoid.h"


template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::BESSEL_1841("fa",	1/299.152813,	6377397.155);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::CLARKE_1880("fa",	1/293.4663,	6378249.145);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::SK42_1940("fa",	1/298.3,	6378245);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::EVEREST_1956("fa",	1/300.8017,	6377301.243);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::AUSTRALIA_1965("fa",1/298.25,	6378160);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::SUSA_1969("fa",	1/298.25,	6378160);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::IAG67_1967("fa",	1/298.247167,	6378160);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::WGS72_1972("fa",	1/298.26,	6378135);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::IAU76_1976("fa",	1/298.257,	6378140);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::GRS80_1979("fa",	1/298.257222101,6378137);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::WGS84_1986("fa",	1/298.257223563,6378137);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::IERS_1989("fa",	1/298.257,	6378136);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::PZ90_1990("fa",	1/298.257839303,6378136);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::IERS_2003("fa",	1/298.25642,	6378136.6);
template <class __TYPE>
mEllipsoid<__TYPE>
mEllipsoid<__TYPE>::Earth::GSK2011_2011("fa",	1/298.2564151,	6378136.5);



template class mEllipsoid<float>;
template class mEllipsoid<double>;
template class mEllipsoid<long double>;

