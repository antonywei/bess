//
// Created by whr on 7/16/19.
//

#ifndef BESS_MODULES_MYACL_H_
#define BESS_MODULES_MYACL_H_

#include <vector>

#include "../module.h"
#include "../pb/module_msg.pb.h"
#include "../utils/ip.h"

using bess::utils::be16_t;
using bess::utils::be32_t;
using bess::utils::Ipv4Prefix;

class MYACL final : public Module{
  public:
    struct MYACLRule {
        bool Match(be32_t sip, be32_t dip, be16_t sport, be16_t dport) const{
            return src_ip.Match(sip) && dst_ip.Match(dip) &&
                   (src_port == be16_t(0)|| src_port == sport) &&
                   (dst_port == be16_t(0)|| dst_port == dport);
        }
        Ipv4Prefix src_ip;
        Ipv4Prefix dst_ip;
        be16_t  src_port;
        be16_t  dst_port;
        bool transfer;
    };

    static const Commands cmds;

    MYACL():Module() { max_allowed_workers_ = Worker::kMaxWorkers;}

    CommandResponse Init(const bess::pb::MYACLArg &arg);
    void ProcessBatch(Context *ctx, bess::PacketBatch *batch) override;
    CommandResponse CommandAdd(const bess::pb::MYACLArg &arg);
    CommandResponse CommandClear(const bess::pb::EmptyArg &arg);
private:
    std::vector<MYACLRule> rules_;
};

#endif

