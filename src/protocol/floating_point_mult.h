#include "protocol/fixed_point.h"
#include "protocol/floating_point.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/sign.h"

template <int lf, int le> struct PI_float_mult_intermediate {
    // input x.t, y.t, output zhat1
    PI_fixed_mult_intermediate<2, lf, lf + 3> fixed_mult_intermediate;
    // input zhat1 with lf+2 bits, output c
    PI_sign_intermediate<lf + 2, (ShareValue(1) << (lf + 3))> sign_intermediate;
    MSSshare<lf + 3> zhat1_lf_3;             // zhat1 with lf+3 bits
    MSSshare_p c{ShareValue(1) << (lf + 3)}; // c with lf+3 bits
    MSSshare_mul_res<lf + 3> zhat2_lf_3;     // zhat2 with lf+3 bits, zhat2 = zhat1 *(2-c)
    MSSshare_mul_res<1> bx_mul_by;           // x.b * y.b

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif
};

template <int lf, int le>
void PI_float_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_float_mult_intermediate<lf, le> &intermediate,
                              FLTshare<lf, le> *input_x, FLTshare<lf, le> *input_y,
                              FLTshare<lf, le> *output_z);

template <int lf, int le>
void PI_float_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_float_mult_intermediate<lf, le> &intermediate, FLTshare<lf, le> *input_x,
                   FLTshare<lf, le> *input_y, FLTshare<lf, le> *output_z);

#include "protocol/floating_point_mult.tpp"