//
// Created by whr on 7/18/19.
//

#include "MVsketch.h"
#include "dump.h"

#include <cmath>
#include <iostream>
#include "../utils/ether.h"
#include "../utils/ip.h"
#include "../utils/udp.h"
#include <string>
#include <arpa/inet.h>
const Commands MVsketch::cmds = {
        {
                "set_para","MVsketchArg",MODULE_CMD_FUNC(&MVsketch::CommandSetPara),
                                                                                    Command::THREAD_UNSAFE},
        {
                "get_val", "MVsketchCommandGetValArg",
                                         MODULE_CMD_FUNC(&MVsketch::CommandGetVal), Command::THREAD_UNSAFE}
};


//function to set list[(flow_key,val_tp)] const T maybe should be change to a special vector
void Debug_Module_packet(unsigned char* key,std::string message){
    std::cout<<"*******debug module******"<<message<<std::endl;
    uint32_t src_ip = *(reinterpret_cast<uint32_t *>(key));
    uint32_t dst_ip = *(reinterpret_cast<uint32_t *>(key + 4));
    uint32_t src_port = *(reinterpret_cast<uint16_t *>(key + 8));
    uint32_t dst_port = *(reinterpret_cast<uint16_t *>(key + 10));
    uint32_t proto = *(reinterpret_cast<uint8_t *>(key+12));
    in_addr saddr;
    saddr.s_addr = src_ip;
    char *src_ip_a = inet_ntoa(saddr);
    std::cout<<"src ip "<<src_ip_a<<std::endl;
    in_addr daddr ;
    daddr.s_addr = dst_ip;
    char *dst_ip_a = inet_ntoa(daddr);
    std::cout<<"dst ip "<<dst_ip_a<<std::endl;
    std::cout<<"src port  "<<src_port<<std::endl;
    std::cout<<"dst port  "<<dst_port<<std::endl;
    std::cout<<"proto  "<<proto<<std::endl;
    //debug final
}
static void SetTrafficInfo(key_tp flow_key,val_tp var,bess::pb::MVsketchCommandGetValResponse * Response){
    bess::pb::MVsketchCommandGetValResponse::TrafficInfo *r = Response->add_flow_list();
    uint32_t src_ip = *(reinterpret_cast<uint32_t *>(flow_key.key));
    uint32_t dst_ip = *(reinterpret_cast<uint32_t *>(flow_key.key + 4));
    uint32_t src_port = *(reinterpret_cast<uint16_t *>(flow_key.key + 8));
    uint32_t dst_port = *(reinterpret_cast<uint16_t *>(flow_key.key + 10));
    uint32_t proto = *(reinterpret_cast<uint8_t *>(flow_key.key + 12));
    Debug_Module_packet(flow_key.key,"*****debug Set Traffic Info******");
    r->set_src_ip(src_ip);
    r->set_dst_ip(dst_ip);
    r->set_src_port(src_port);
    r->set_dst_port(dst_port);
    r->set_proto(proto);
    r->set_count(var);
}

CommandResponse MVsketch::Init(const bess::pb::MVsketchArg &arg){
    int depth = arg.depth();  //the para define in protobuf file should be called by function
    int width = arg.width();
    if (depth<=0 || width<=0){
        return CommandFailure(EINVAL,"row depth threshold must larger than 0");
    }
    Sum_total = 0;
    mv_.depth = depth;
    mv_.width = width;
    mv_.lgn = 8*LGN;
    mv_.sum = 0;
    mv_.counts = new SBucket *[depth*width];
    for (int i = 0; i < depth*width; i++) {
        mv_.counts[i] = (SBucket*)calloc(1, sizeof(SBucket));
        memset(mv_.counts[i], 0, sizeof(SBucket));
        mv_.counts[i]->key[0] = '\0';
    }
    mv_.hardner = new unsigned long[depth];
    char name[] = "MVSketch";
    unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
    for (int i = 0; i < depth; i++) {
        mv_.hardner[i] = GenHashSeed(seed++);
    }

    return CommandSuccess();
};
// CommandSetPara only use to read the Set Paras
CommandResponse MVsketch::CommandSetPara(const bess::pb::MVsketchArg &arg){
    Init(arg);
    return CommandSuccess();
};
CommandResponse MVsketch::CommandClear(const bess::pb::EmptyArg &) {
    return CommandSuccess();
}

CommandResponse MVsketch::CommandGetVal(const bess::pb::MVsketchCommandGetValArg &arg) {
    bess::pb::MVsketchCommandGetValResponse r;
    //flow_key test_key = MVvector_[1];
    double threshold = arg.threshold();
    val_tp Threshold = threshold*Sum_total;
    std::cout<<"*****Debug for GetVal*****"<<threshold<<std::endl;
    std::cout<<"threshold "<<threshold<<std::endl;
    std::cout<<"sum "<<Sum_total<<std::endl;
    std::vector<std::pair<key_tp, val_tp> > results;
    results.clear();
    Query(Threshold,results);
    for(auto res = results.begin();res != results.end();res++ ){
        SetTrafficInfo(res->first,res->second,&r);
    }
    Reset();
    return CommandSuccess(r);
    //need to add traffic_data as private data of class ... so that it can be read in this function
}
void MVsketch::Reset(){
    for (int i = 0; i < mv_.depth*mv_.width; i++) {
        mv_.counts[i]->count = 0;
        mv_.counts[i]->sum = 0;
    }
    Sum_total = 0;
}
void MVsketch::Update(unsigned char* key, val_tp val){
    Sum_total += val;
    mv_.sum += val;
    // mv_= {sum = 4544, counts = 0x730bb0, depth = 4, width = 1366,
    //       lgn = 64, hash = 0x766180, scale = 0x7661b0, hardner = 0x7661e0}
    // mv is the main mvsketch table
    Debug_Module_packet(key,"*****update begin********");
    unsigned long bucket = 0;
    int keylen = mv_.lgn/8;
    for (int i = 0; i < mv_.depth; i++) {
        bucket = MurmurHash64A(key, keylen, mv_.hardner[i]) % mv_.width;  // decide which col to store the candidate vote
        //key =(unsigned char *) 0x7fffffffe051 "\241E0ۡE-\005\326\065\237\003\006\334\005LM\001"
        //keylen = 8
        //key is the flow key, but bucket is the hash_result%width define which bucket to store the key
        int index = i*mv_.width+bucket;  //real index, row i and the buckets col
        SBucket *sbucket = mv_.counts[index];
        sbucket->sum += val;
        if (sbucket->key[0] == '\0') {
            memcpy(sbucket->key, key, keylen);  //set a new candidate key
            //Debug_Module_packet(sbucket->key,"*****new a candidate key******");
            sbucket->count = val;  //set a new candidate vote value
        } else if(memcmp(key, sbucket->key, keylen) == 0) {  //add vote
            //Debug_Module_packet(key,"*****add vote debug******");
            sbucket->count += val;
            //printf("******add vote now the sbucket count = %ld \n",sbucket->count);
        } else {
            sbucket->count -= val;     //devote
            //printf("******delet vote debug now the sbucket count = %ld \n",sbucket->count);
            if (mv_likely(sbucket->count < 0)) {
                memcpy(sbucket->key, key, keylen);   //get a new candidate vote
                sbucket->count = -sbucket->count;
                //printf("******modify vote now the sbucket count = %ld \n",sbucket->count);
            }
        }
    }
}

void MVsketch::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
    //ctx Set by task scheduler, read by modules,may be a connection to another module
    //packetBatch maybe a batch for store incoming packet
    const int ip_offset = 14;
    gate_idx_t incoming_gate = ctx ->current_igate;
    int cnt = batch->cnt();  //
    for (int i = 0; i < cnt ; i++){
        bess::Packet *snb = batch->pkts()[i];
        char *head = snb->head_data<char *>();
        tuple_t t;
        t.key.src_ip = *(reinterpret_cast<uint32_t *>(head + ip_offset + 12));
        t.key.dst_ip = *(reinterpret_cast<uint32_t *>(head + ip_offset + 16));
        t.key.src_port = *(reinterpret_cast<uint16_t *>(head + ip_offset + 20));
        t.key.dst_port = *(reinterpret_cast<uint16_t *>(head + ip_offset + 22));
        t.key.protocol = *(reinterpret_cast<uint8_t *>(head + ip_offset + 9)); /* ip_proto */
        t.size = 1;
        /*debug code

        std::cout << "****** debug in process batch 1 *******" <<std::endl;
        uint8_t srcIp1 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 12));
        uint8_t srcIp2 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 13));
        uint8_t srcIp3 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 14));
        uint8_t srcIp4 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 15));

        uint8_t dstIp1 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 16));
        uint8_t dstIp2 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 17));
        uint8_t dstIp3 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 18));
        uint8_t dstIp4 = *(reinterpret_cast<uint8_t *>(head + ip_offset + 19));

        uint16_t srcPort = *(reinterpret_cast<uint16_t *>(head + ip_offset + 20));
        uint16_t dstPort = *(reinterpret_cast<uint16_t *>(head + ip_offset + 22));

        printf("The srcIp is: %d.%d.%d.%d \n", +srcIp1, +srcIp2, +srcIp3, +srcIp4);
        printf("The dstIp is: %d.%d.%d.%d \n", +dstIp1, +dstIp2, +dstIp3, +dstIp4);
        printf("The srcPort is: %d \n", +srcPort);
        printf("The dstPort is: %d \n", +dstPort);

        std::cout<<"****** debug in PROCESS BATCH *******";
        in_addr saddr;
        saddr.s_addr = t.key.src_ip;
        char *src_ip_a = inet_ntoa(saddr);
        std::cout<<"src ip "<<src_ip_a<<std::endl;
        in_addr daddr ;
        daddr.s_addr = t.key.dst_ip;
        char *dst_ip_a = inet_ntoa(daddr);
        std::cout<<"dst ip "<<dst_ip_a<<std::endl;
        std::cout<<"src port  "<<std::hex<<t.key.src_port<<std::endl;
        std::cout<<"dst port  "<<std::hex<<t.key.dst_port<<std::endl;
        //std::cout<<"proto  "<<t.key.protocol<<std::endl;
         //t.size = *(reinterpret_cast<uint16_t *>(head + ip_offset + 2));
        debug complete
        */
        Update((unsigned char*)&(t.key), (val_tp)t.size);
        EmitPacket(ctx, snb, incoming_gate);
    }
}

void MVsketch::Query(val_tp thresh, std::vector<std::pair<key_tp, val_tp> >&results) {
    myset res;
    for (int i = 0; i < mv_.width*mv_.depth; i++) {
        if (mv_.counts[i]->sum > thresh) {
            key_tp reskey;
            memcpy(reskey.key, mv_.counts[i]->key, mv_.lgn/8);
            res.insert(reskey);
        }
    }
/*
    for (int i = 0; i < mv_.width; i++) {
        if (mv_.counts[i]->sum > thresh) {
            Debug_Module_packet(mv_.counts[i]->key,"****debug in query****");
        }
    }
*/
    for (auto it = res.begin(); it != res.end(); it++) {
        val_tp resval = 0;
        for (int j = 0; j < mv_.depth; j++) {
            unsigned long bucket = MurmurHash64A((*it).key, mv_.lgn/8, mv_.hardner[j]) % mv_.width;
            unsigned long index = j*mv_.width+bucket;
            val_tp tempval = 0;
            if (memcmp(mv_.counts[index]->key, (*it).key, mv_.lgn/8) == 0) {
                tempval = (mv_.counts[index]->sum - mv_.counts[index]->count)/2 + mv_.counts[index]->count;
            } else {
                tempval = (mv_.counts[index]->sum - mv_.counts[index]->count)/2;
            }
            if (j == 0) resval = tempval;
            else resval = std::min(tempval, resval);
        }
        if (resval > thresh ) {
            key_tp key;
            memcpy(key.key, (*it).key, mv_.lgn/8);
            std::pair<key_tp, val_tp> node;
            node.first = key;
            node.second = resval;
            results.push_back(node);
        }
    }
}


ADD_MODULE(MVsketch,"MVsketch","MVsketch module from antonywei")

