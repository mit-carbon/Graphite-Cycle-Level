#include "finite_buffer_network_model_emesh.h"

class FiniteBufferNetworkModelEMeshBroadcastTree : public FiniteBufferNetworkModelEMesh
{
   public:
      FiniteBufferNetworkModelEMeshBroadcastTree(Network* net, SInt32 network_id):
         FiniteBufferNetworkModelEMesh(net, network_id, true)
      {}
      ~FiniteBufferNetworkModelEMeshBroadcastTree() {}
};
