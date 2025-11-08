#pragma once
#include "config/config.h"
#include "protocol/addictive_share_p.h"
#include "protocol/masked_RSS_p.h"
#include "utils/misc.h"
#include "utils/permutation.h"

/*
 * 功能：找到当前比特位分享对应的第一个出现1的位置，并将该位置作为在环p2上的加法分享输出
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param input_bit_shares:
 * 每个元素是一个比特位，在P1与P2之间，环p1上的加法分享，p1必须是一个素数，且p1>l
 * @param output_result: 输出结果是一个在P1与P2之间，环p2上的加法分享，p2不要求是素数，但是要求p2>l
 */
void PI_detect(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p *> &input_bit_shares, ADDshare_p *output_result);

/*
 * 功能：找到当前比特位分享对应的第一个出现1的位置，并将该位置（包括自身）之前的位置设为1，其余设为0，并将结果作为在环p2上的加法分享输出
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param input_bit_shares:
 * 每个元素是一个比特位，在P1与P2之间，环p1上的加法分享，p1必须是一个素数，且p1>l
 * @param output_result: 输出结果是一个在P1与P2之间，环p2上的加法分享，p2不要求是素数，但是要求p2>l
 */
void PI_prefix(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p *> &input_bit_shares,
               std::vector<ADDshare_p *> &output_result);

/*
 * 功能：找到当前比特位分享对应的第一个出现1的位置，并将该位置（包括自身）之前的位置设为b，其余设为0，并将结果作为在环p2上的加法分享输出
 * @param party_id: 参与方id，0/1/2
 * @param PRGs: 用于生成随机数的伪随机生成器数组，
 * P0.PRGs[0] <----sync----> P1.PRGs[0],
 * P0.PRGs[1] <----sync----> P2.PRGs[0],
 * P1.PRGs[1] <----sync----> P2.PRGs[1]
 * @param netio: 多方通信接口
 * @param input_bit_shares:
 * 每个元素是一个比特位，在P1与P2之间，环p1上的加法分享，p1必须是一个素数，且p1>l
 * @param b: 环p2上的MSS分享
 * @param output_result: 输出结果是一个在P1与P2之间，环p2上的加法分享，p2不要求是素数，但是要求p2>l
 */
void PI_prefix_b(const int party_id, std::vector<PRGSync> &PRGs, NetIOMP &netio,
               std::vector<ADDshare_p *> &input_bit_shares, MSSshare_p *input_b,
               std::vector<ADDshare_p *> &output_result);