#pragma once

#include "network.h"
#include "core.h"

class NetworkModelMagic : public NetworkModel
{
public:
   NetworkModelMagic(Network *net, SInt32 network_id);
   ~NetworkModelMagic();

   volatile float getFrequency() { return 1.0; }

   UInt32 computeAction(const NetPacket& pkt);
   void routePacket(const NetPacket &pkt, std::vector<Hop> &nextHops);
   void processReceivedPacket(NetPacket& pkt);

   void outputSummary(std::ostream &out);

   void reset() {}
};
