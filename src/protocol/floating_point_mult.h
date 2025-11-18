#include "protocol/fixed_point.h"
#include "protocol/floating_point.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/sign.h"

struct PI_float_mult_intermediate {
    int lf;
    int le;
    // input x.t, y.t, output zhat1
    PI_fixed_mult_intermediate fixed_mult_intermediate;
    // input zhat1 with lf+2 bits, output c
    PI_sign_intermediate sign_intermediate;
    MSSshare zhat1_lf_3;         // zhat1 with lf+3 bits
    MSSshare_p c;                // c with lf+3 bits
    MSSshare_mul_res zhat2_lf_3; // zhat2 with lf+3 bits, zhat2 = zhat1 *(2-c)
    MSSshare_mul_res bx_mul_by;  // x.b * y.b

#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_float_mult_intermediate(int lf, int le)
        : fixed_mult_intermediate(2, lf, lf + 3),
          sign_intermediate(lf + 2, (ShareValue(1) << (lf + 3))), zhat1_lf_3(lf + 3),
          c(ShareValue(1) << (lf + 3)), zhat2_lf_3(lf + 3), bx_mul_by(1) {
        this->lf = lf;
        this->le = le;
    }
};

void PI_float_mult_preprocess(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                              PI_float_mult_intermediate &intermediate, FLTshare *input_x,
                              FLTshare *input_y, FLTshare *output_z);

void PI_float_mult(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                   PI_float_mult_intermediate &intermediate, FLTshare *input_x, FLTshare *input_y,
                   FLTshare *output_z);

void PI_float_mult_vec(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
                       std::vector<PI_float_mult_intermediate *> &intermediate_vec,
                       std::vector<FLTshare *> &input_x_vec, std::vector<FLTshare *> &input_y_vec,
                       std::vector<FLTshare *> &output_z_vec);

#include "protocol/floating_point_mult.tpp"