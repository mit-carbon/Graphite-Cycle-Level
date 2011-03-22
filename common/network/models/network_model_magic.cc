#include "network.h"
#include "network_model_magic.h"
#include "log.h"

NetworkModelMagic::NetworkModelMagic(Network *net, SInt32 network_id) : 
   NetworkModel(net, network_id),
   _enabled(false)
{
   initializePerformanceCounters();
}

NetworkModelMagic::~NetworkModelMagic()
{}

void
NetworkModelMagic::initializePerformanceCounters()
{
   _num_packets = 0;
   _num_bytes = 0;
}

UInt32
NetworkModelMagic::computeAction(const NetPacket& pkt)
{
   core_id_t core_id = getNetwork()->getCore()->getId();
   LOG_ASSERT_ERROR((pkt.receiver == NetPacket::BROADCAST) || (pkt.receiver == core_id), \
         "pkt.receiver(%i), core_id(%i)", pkt.receiver, core_id);

   return RoutingAction::RECEIVE;
}

void
NetworkModelMagic::routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops)
{
   // A latency of '1'
   if (pkt.receiver == NetPacket::BROADCAST)
   {
      UInt32 total_cores = Config::getSingleton()->getTotalCores();
   
      for (SInt32 i = 0; i < (SInt32) total_cores; i++)
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
NetworkModelMagic::processReceivedPacket(NetPacket &pkt)
{
   ScopedLock sl(_lock);

   core_id_t requester = getNetwork()->getRequester(pkt);
   LOG_ASSERT_ERROR((requester >= 0) && (requester < (core_id_t) Config::getSingleton()->getTotalCores()),
         "requester(%i)", requester);

   if ( (!_enabled) || (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()) )
      return;

   UInt32 pkt_length = getNetwork()->getModeledLength(pkt);
   _num_packets ++;
   _num_bytes += pkt_length;
}

void NetworkModelMagic::outputSummary(std::ostream &out)
{
   out << "    num packets received: " << _num_packets << std::endl;
   out << "    num bytes received: " << _num_bytes << std::endl;
}
