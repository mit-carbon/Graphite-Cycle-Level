#include "finite_buffer_network_model_emesh.h"

class FiniteBufferNetworkModelEMeshBasic : public FiniteBufferNetworkModelEMesh
{
   public:
      FiniteBufferNetworkModelEMeshBasic(Network* net, SInt32 network_id):
         FiniteBufferNetworkModelEMesh(net, network_id, false)
      {}
      ~FiniteBufferNetworkModelEMeshBasic() {}
};
