#include "network.h"
#include "network_model_magic.h"
#include "log.h"

NetworkModelMagic::NetworkModelMagic(Network *net, SInt32 network_id)
   : NetworkModel(net, network_id)
{
   _flit_width = INFINITE_BANDWIDTH;
}

NetworkModelMagic::~NetworkModelMagic()
{}

UInt32
NetworkModelMagic::computeAction(const NetPacket& pkt)
{
   LOG_ASSERT_ERROR((pkt.receiver == NetPacket::BROADCAST) || (pkt.receiver == _core_id),
         "pkt.receiver(%i), core_id(%i)", pkt.receiver, _core_id);

   return RoutingAction::RECEIVE;
}

void
NetworkModelMagic::routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops)
{
   // A latency of '1'
   if (pkt.receiver == NetPacket::BROADCAST)
   {
      SInt32 total_cores = Config::getSingleton()->getTotalCores();
   
      for (SInt32 i = 0; i < total_cores; i++)
      {
         Hop h;
         h.final_dest = NetPacket::BROADCAST;
         h.next_dest = i;
         h.time = pkt.time + 1;

         nextHops.push_back(h);
      }
   }
   else
   {
      Hop h;
      h.final_dest = pkt.receiver;
      h.next_dest = pkt.receiver;
      h.time = pkt.time + 1;

      nextHops.push_back(h);
   }
}

void
NetworkModelMagic::processReceivedPacket(const NetPacket* packet)
{
   if (_enabled)
      updatePacketReceiveStatistics(packet, 1);
}

void NetworkModelMagic::outputSummary(std::ostream &out)
{
   NetworkModel::outputSummary(out);
}
