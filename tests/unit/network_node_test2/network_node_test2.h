#pragma once

#include <list>
#include <vector>
using std::list;
using std::vector;
#include "network.h"
#include "flit.h"
#include "buffer_management_msg.h"
#include "router.h"
#include "packetize.h"

list<NetPacket*> createNetPacketList();
void addFlit(list<NetPacket*>& net_packet_list, Flit::Type flit_type, UInt64 time, SInt32 length, Router::Id sender_router_id, Router::Id receiver_router_id, SInt32 num_output_endpoints = 0, UnstructuredBuffer* output_endpoints_ptr = NULL);
void addCreditMsg(list<NetPacket*>& net_packet_list, UInt64 time, SInt32 num_credits, Router::Id sender_router_id, Router::Id receiver_router_id);
void addOnOffMsg(list<NetPacket*>& net_packet_list, UInt64 time, bool on_off_status, Router::Id sender_router_id, Router::Id receiver_router_id);

void destroyNetPacketList(list<NetPacket*>& net_packet_list);
void destroyNetPacket(NetPacket* net_packet);

void printNetPacketList(list<NetPacket*>& net_packet_list, bool is_input_msg = false);
void printNetPacket(NetPacket* net_packet, bool is_input_msg = false);
