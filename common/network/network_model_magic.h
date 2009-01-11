#ifndef NETWORK_MODEL_MAGIC_H
#define NETWORK_MODEL_MAGIC_H

#include "network.h"
#include "core.h"

class NetworkModelMagic : public NetworkModel
{
   UInt64 _bytesSent;
 public:
   NetworkModelMagic(Network *net) : NetworkModel(net), _bytesSent(0) { }
   ~NetworkModelMagic() { }

   void routePacket(const NetPacket &pkt,
                    std::vector<Hop> &nextHops)
   {
      Hop h;
      h.dest = pkt.receiver;
      h.time = _network->getCore()->getPerfModel()->getCycleCount();
      nextHops.push_back(h);

      _bytesSent += pkt.length;
   }

   void outputSummary(std::ostream &out)
   {
      out << "MAGIC NETWORK: " << _bytesSent << " bytes sent.\n";
   }
};

#endif
