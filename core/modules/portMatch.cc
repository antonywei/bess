//
// Created by whr on 7/30/19.
//

#include "portMatch.h"
const Commands portMatch::cmds = {
        {
                "set_para","portMatchArg",MODULE_CMD_FUNC(&portMatch::CommandSetPara),
                                                                                    Command::THREAD_UNSAFE},
        {
                "add", "portMatchCommandAddArg",
                                         MODULE_CMD_FUNC(&portMatch::CommandAdd), Command::THREAD_SAFE}
};

CommandResponse portMatch::Init(const bess::pb::portMatchArg &arg) {
    active_table = 0;
    for(auto it=arg.port_list().begin();it!=arg.port_list().end();it++) {
        portMatch_t matchtable;
        matchtable.portKey.src_port = it->src_port();
        matchtable.portKey.dst_port = it->dst_port();
        matchtable.gate = it->gate();
        matchtable.count = 0;
        Active_pM()->push_front(matchtable);
        Backup_pM()->push_front(matchtable);
    }
    return CommandSuccess();
}
CommandResponse portMatch::CommandSetPara(const bess::pb::portMatchArg &arg) {
    Init(arg);
    return CommandSuccess();
}

void portMatch::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
    const int ip_offset = 14;
    gate_idx_t incoming_gate = ctx->current_igate;
    int cnt = batch->cnt();  //
    for (int i = 0; i < cnt; i++) {
        bess::Packet *snb = batch->pkts()[i];
        char *head = snb->head_data<char *>();
        uint8_t src_port_head = *(reinterpret_cast<uint8_t *>(head + ip_offset + 20));
        uint8_t src_port_tail = *(reinterpret_cast<uint8_t *>(head + ip_offset + 21));
        uint8_t dst_port_head = *(reinterpret_cast<uint8_t *>(head + ip_offset + 22));
        uint8_t dst_port_tail = *(reinterpret_cast<uint8_t *>(head + ip_offset + 23));
        portKey_t pKey;
        pKey.dst_port = (src_port_head << 8) + src_port_tail;
        pKey.src_port = (dst_port_head << 8) + dst_port_tail;
        uint32_t gate = getGate(pKey);
        //send packet
    }
}
uint32_t portMatch::getGate(portKey_t pKey){
    uint32_t gate = DEFAULTGATE;
    for(auto it = Active_pM()->begin();it!=Active_pM()->end();it++){ //in this place it is a pointer
        if(pKey.src_port == it->portKey.src_port && pKey.dst_port == it->portKey.dst_port){
            gate = it->gate;
            it->count ++;
            return gate;
        }
    }
    return gate;
}

CommandResponse portMatch::CommandAdd(const bess::pb::portMatchCommandAddArg &arg) {
    //swap T1 T2 -> clear mini table of T1 -> add table T1 -> clear count T1 ->
    //swap table -> clear count T2 and delete table T2 -> add table T2
    uint64_t thresh = arg.threshold();
    //swaptable
    //clear mini table and delete count
    //add table
    //add table

}
void portMatch::Add(portKey_t pKey,uint32_t gate) {
    portMatch_t pm;
    pm.portKey.dst_port = pKey.dst_port;
    pm.portKey.src_port = pKey.src_port;
    pm.gate = gate;
    Backup_pM()->push_front(pm);
}
pm portMatch::cleanMiniFlow(uint64_t threshold) {
    pm del_table;
    for(auto it=Backup_pM()->begin();it!=Backup_pM()->end();++it){
        if(it->count<threshold){
            del_table.push_front(*it);
            it = Backup_pM()->erase(it);
        }
    }
    return del_table;
}

void portMatch::swapTable(){
    active_table = !active_table;
}

void portMatch::delTable(pm del_table){ //bug in same port and different gate?
    for(auto it=Backup_pM()->begin();it!=Backup_pM()->end();++it){
        if(it->portKey.src_port){
            del_table.push_front(*it);
            it = Backup_pM()->erase(it);
        }
    }
}

void portMatch::loop() {
    return;
}