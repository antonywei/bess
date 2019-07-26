//
// Created by whr on 7/18/19.
//

#ifndef BESS_MVSKETCH_H
#define BESS_MVSKETCH_H

#include <vector>
#include "../module.h"
#include "../pb/module_msg.pb.h"
#include "../utils/ip.h"
#include "../utils/MVsketchTypeDefine.h"
#include "../utils/hash.h"
#define mv_likely(x) __builtin_expect ((x), 1)
//typedef std::vector<std::pair<key_tp, val_tp> > myvector;

class MVsketch final : public Module {

    typedef std::unordered_set<key_tp, key_tp_hash, key_tp_eq> myset;

    typedef std::unordered_map<key_tp, val_tp, key_tp_hash, key_tp_eq> mymap;

    typedef std::vector<std::pair<key_tp, val_tp> > myvector;

    typedef struct SBUCKET_type {
        //Total sum V(i, j)
        //typedef uint64_t val_tp;
        val_tp sum;  //total packets
        long count;  //vote count
        //previous define LGN = 8
        unsigned char key[LGN]; // char[8] to store 64bit hash
    } SBucket;

    struct MV_type {
        //Counter to count total degree
        val_tp sum;
        //counts is the pointer to define the buckets
        SBucket **counts;
        //Outer sketch depth and width
        int depth;
        int width;
        //# key word bits
        int lgn;
        unsigned long *hardner;
    };
public:
    CommandResponse Init(const bess::pb::MVsketchArg &arg);
    void ProcessBatch(Context *ctx, bess::PacketBatch *batch) override;
    void Update(unsigned char* key, val_tp value);
    CommandResponse CommandSetPara(const bess::pb::MVsketchArg &arg);
    CommandResponse CommandClear(const bess::pb::EmptyArg &) ;
    CommandResponse CommandGetVal(const bess::pb::MVsketchCommandGetValArg &arg);
    void Query(val_tp thresh, std::vector<std::pair<key_tp, val_tp> >&results);
    void Reset();
    static const Commands cmds;

private:
    MV_type mv_; //MVsketch datastructure
    val_tp Sum_total;  //store the sum of all the packet size
};


#endif //BESS_MVSKETCH_H
