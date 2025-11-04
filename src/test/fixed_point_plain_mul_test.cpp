#include "utils/prg_sync.h"
#include <bitset>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
const int l = 6;
const int lf = 3;
const uint MASK_l = (1 << l) - 1;
const uint MASK_l_1 = (1 << (l - 1)) - 1;
const uint MASK_llf = (1 << (l + lf)) - 1;

using namespace std;

struct share {
    uint m, r1, r2;

    int cal_w(int ll) {
        int mask = (1 << ll) - 1;
        auto total = (m & mask) + (r1 & mask) + (r2 & mask);
        int w = 0;
        while (total > mask) {
            total -= (mask + 1);
            w++;
        }
        return w;
    }

    uint recon() { return (m + r1 + r2) & MASK_llf; }
};

const int test_num = 1000;
share s1[test_num], s2[test_num], s3[test_num];

int main() {

    for (int i = 0; i < test_num; i++) {
        do {
            auto v = gen_seed();
            s1[i].m = *((uint *)&v);
            s1[i].m = s1[i].m & MASK_llf;
            v = gen_seed();
            s1[i].r1 = *((uint *)&v);
            s1[i].r1 = s1[i].r1 & MASK_llf;
            v = gen_seed();
            s1[i].r2 = *((uint *)&v);
            s1[i].r2 = s1[i].r2 & MASK_llf;
        } while (!(s1[i].recon() < (1 << (l - 2))) &&
                 !(s1[i].recon() >= ((1 << l + lf) - (1 << (l - 2)))));
        cout << bitset<l + lf>(s1[i].recon() + (1 << (l - 2))) << endl;
    }

    for (int i = 0; i < test_num; i++) {
        do {
            auto v = gen_seed();
            s2[i].m = *((uint *)&v);
            s2[i].m = s2[i].m & MASK_llf;
            v = gen_seed();
            s2[i].r1 = *((uint *)&v);
            s2[i].r1 = s2[i].r1 & MASK_llf;
            v = gen_seed();
            s2[i].r2 = *((uint *)&v);
            s2[i].r2 = s2[i].r2 & MASK_llf;
        } while (!(s2[i].recon() < (1 << (l - 2))) &&
                 !(s2[i].recon() >= ((1 << l + lf) - (1 << (l - 2)))));
        // cout << bitset<l + lf>(s2[i].recon() + (1 << (l - 2))) << endl;
    }
    // 随机生成x,y的share，要求x,y在[ -2^(l-2), 2^(l-2) )范围内
    // 即重建的x,y值属于[ 0, 2^(l-2) ) U [ 2^(l+lf)-(2^(l-2)), 2^(l+lf) )

    for (int i = 0; i < test_num; i++) {
        // ts1, ts2: 带符号的真实值
        int ts1 = s1[i].recon();
        ts1 = ts1 + (1 << (l - 2));
        ts1 = ts1 & MASK_l;
        ts1 = ts1 - (1 << (l - 2));
        int ts2 = s2[i].recon();
        ts2 = ts2 + (1 << (l - 2));
        ts2 = ts2 & MASK_l;
        ts2 = ts2 - (1 << (l - 2));
        int tres = ts1 * ts2;

        // ss1, ss2: s1, s2将m部分 +2^(l-2) 后的share
        int mp1 = (s1[i].m + (1 << (l - 2))) & MASK_llf;
        int mp2 = (s2[i].m + (1 << (l - 2))) & MASK_llf;
        auto ss1 = s1[i], ss2 = s2[i];
        ss1.m = mp1;
        ss2.m = mp2;
        
        // w1, w2: ss1, ss2的wrap bit数量，统计的是低l位的wrap bit
        int w1 = (ss1.r1 & MASK_l) + (ss1.r2 & MASK_l) > MASK_l ? 1 : 0;
        int tmp1 = (ss1.r1 + ss1.r2) & MASK_l;
        w1 += ((tmp1 & (1 << (l - 1))) or (ss1.m & (1 << (l - 1)))) ? 1 : 0;
        int w2 = (ss2.r1 & MASK_l) + (ss2.r2 & MASK_l) > MASK_l ? 1 : 0;
        int tmp2 = (ss2.r1 + ss2.r2) & MASK_l;
        w2 += ((tmp2 & (1 << (l - 1))) or (ss2.m & (1 << (l - 1)))) ? 1 : 0;

        // int w1 = ss1.cal_w(l);
        // int w2 = ss2.cal_w(l);

        // cs1, cs2: 计算方式为：m, r1, r2的低l位不取模直接相加，减去 w * 2^l，再减去 2^(l-2)，带符号
        int cs1 = (ss1.m & MASK_l) + (ss1.r1 & MASK_l) + (ss1.r2 & MASK_l) - w1 * (MASK_l + 1) -
                  (1 << (l - 2));
        int cs2 = (ss2.m & MASK_l) + (ss2.r1 & MASK_l) + (ss2.r2 & MASK_l) - w2 * (MASK_l + 1) -
                  (1 << (l - 2));

        // int cs1 = ss1.m + ss1.r1 + ss1.r2 - w1 * (MASK_llf + 1) - (1 << (l - 2));
        // int cs2 = ss2.m + ss2.r1 + ss2.r2 - w2 * (MASK_llf + 1) - (1 << (l - 2));
        int cres = cs1 * cs2;
        // if (true) {
        if (tres != cres) {
            cout << "i:" << i << endl;
            cout << "s1.m:" << s1[i].m << ", s1.r1:" << s1[i].r1 << ", s1.r2:" << s1[i].r2 << endl;
            cout << "s2.m:" << s2[i].m << ", s2.r1:" << s2[i].r1 << ", s2.r2:" << s2[i].r2 << endl;
            cout << "ss1.m:" << ss1.m << ", ss1.r1:" << ss1.r1 << ", ss1.r2:" << ss1.r2 << endl;
            cout << "ss2.m:" << ss2.m << ", ss2.r1:" << ss2.r1 << ", ss2.r2:" << ss2.r2 << endl;
            cout << "s1.recon:" << s1[i].recon() << ", s2.recon:" << s2[i].recon() << endl;
            cout << "ss1.recon:" << ss1.recon() << ", ss2.recon:" << ss2.recon() << endl;
            cout << "w1:" << w1 << ", w2:" << w2 << endl;
            cout << "ts1:" << ts1 << ", ts2:" << ts2 << ", tres:" << tres << endl;
            cout << "cs1:" << cs1 << ", cs2:" << cs2 << ", cres:" << cres << endl;
            cout << bitset<2 * l>(tres) << endl;
            cout << bitset<2 * l>(cres) << endl;
            if (tres != cres) {
                cout << "error" << endl;
                return 1;
            }
        }
    }
}