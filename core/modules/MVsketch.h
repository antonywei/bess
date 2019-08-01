//
// Created by whr on 7/18/19.
//
#ifndef BESS_MVSKETCH_H
#define BESS_MVSKETCH_H
#include "../module.h"
#include "../pb/module_msg.pb.h"
#include "../utils/ip.h"
#include "../utils/hash.h"
#include "string.h"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define LGN 13 // if LGN = 13 HASH 5-TUPLE,LGN = 8 HASH ip only
#include "../utils/hash.h"
#include <atomic>
typedef struct key_t_s {
    unsigned char key[LGN];
} key_tp;

typedef uint64_t val_tp;
/**
    * Object for hash
    */

typedef struct {
    /// overloaded operation
    long operator() (const key_tp &k) const { return MurmurHash64A(k.key, LGN, 388650253); }
} key_tp_hash;

typedef struct {
    /// overloaded operation
    bool operator() (const key_tp &x, const key_tp &y) const {
        return memcmp(x.key, y.key, LGN)==0;
    }
} key_tp_eq;

/**
 * IP flow key
 * */
typedef struct __attribute__ ((__packed__)) flow_key {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
} flow_key_t;

/**
 *input data
 * */
typedef struct __attribute__ ((__packed__)) Tuple {
    flow_key_t key;
    uint16_t size;
} tuple_t;

#define BIG_CONSTANT(x) (x##LLU)


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
    MV_type *Active_Mv(){
        return &mv_table[active_table];
    } //using point function to point the data result
    MV_type *Backup_Mv(){
        return &mv_table[!active_table];
    }
    void SwapMvTable(){
        active_table = !active_table;
    } //Swap table to change Mvsketch
    void Query(val_tp thresh, std::vector<std::pair<key_tp, val_tp> >&results);
    void Reset();
    static const Commands cmds;

private:
    //struct MV_type mv_;
    struct MV_type mv_table[2]; //MVsketch datastructure
    std::atomic_bool active_table;
    val_tp Sum_total;  //store the sum of all the packet size
};
//
// Created by whr on 7/21/19.
//
#endif //BESS_MVSKETCH_H


