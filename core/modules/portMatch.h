//
// Created by whr on 7/30/19.
//

#ifndef BESS_PORTMATCH_H
#define BESS_PORTMATCH_H
#include "../module.h"
#include "../pb/module_msg.pb.h"
#include "../utils/ip.h"
#include "../utils/hash.h"
#include <iostream>
#include <list>
#include <iterator>
#include <algorithm>
#include <atomic>
#define DEFAULTGATE 11;
typedef struct portKey{
    uint32_t src_port;
    uint32_t dst_port;
}portKey_t;
typedef struct MatchTable{
    portKey_t portKey;  //portkey include src_port and dst_port
    gate_idx_t gate;     //the gate to send packets
    uint64_t count;    //count
} portMatch_t;
typedef std::list<portMatch_t> pm;
class portMatch final : public Module {
public:
    static const gate_idx_t kNumIGates = 1;
    static const gate_idx_t kNumOGates = 12;
    static const Commands cmds;
    CommandResponse Init(const bess::pb::portMatchArg &arg);
    void ProcessBatch(Context *ctx, bess::PacketBatch *batch) override; //process the packets
    CommandResponse CommandSetPara(const bess::pb::portMatchArg &arg);  //init the variable
    CommandResponse CommandAdd(const bess::pb::portMatchCommandAddArg &arg); //add the table
    //void Add(portKey_t portKey,uint32_t gate);// add only one rules
    pm *Active_pM(){
        return &pM_[active_table];
    } //using point function to point the data result
    pm *Backup_pM(){
        return &pM_[!active_table];  //backup table
    }
    //void Modify(); //modify rules
    void swapTable(); //thread safe ready
    void loop();  //using to display all rules
    void delTable(pm del_table);//delete table
    gate_idx_t getGate(portKey_t pKey);//finde gate in table  using in process batch
    pm cleanMiniFlow(uint64_t threshold); //clean the mini flow
private:
    pm pM_[2]; //full store
    std::atomic_bool active_table; //the flag to show the active table
    uint64_t Sum_total; //count total packets
};

#endif //BESS_PORTMATCH_H
