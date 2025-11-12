#pragma once
#include "config/config.h"
#include "protocol/masked_RSS.h"
#include "protocol/masked_RSS_p.h"
#include "protocol/great.h"
#include "utils/misc.h"

struct PI_floating_add_intermediate {
    int lf;
    int le;

    // //input: x.e , y.e; output: 1 bit
    // PI_great_intermediate great_inter1;
    // //input: x.t , y.t; output: 1 bit
    // PI_great_intermediate great_inter2;
    // //input: zeta1 , lf; output: 1 bit
    // PI_great_intermediate great_inter3;

    
#ifdef DEBUG_MODE
    bool has_preprocess = false;
#endif

    PI_floating_add_intermediate(int lf, int le) {
        this->lf = lf;
        this->le = le;
    }
};