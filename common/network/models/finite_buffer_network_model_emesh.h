#pragma once

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelEMesh : public FiniteBufferNetworkModel
{
   public:
      FiniteBufferNetworkModelEMesh(Network* net, SInt32 network_id, bool broadcast_enabled);
      ~FiniteBufferNetworkModelEMesh();
   
   private:
      enum RouterType
      {
         CORE_INTERFACE = -1,
         EMESH = 0
      };

      // Private Functions
      Router* createRouter();

      void computeEMeshPosition(core_id_t core_id, SInt32& x, SInt32& y);
      core_id_t computeCoreId(SInt32 x, SInt32 y);
      // Static Functions
      static void computeEMeshTopologyParameters(SInt32& emesh_width, SInt32& emesh_height);

      // Main Routing Function
      void computeOutputEndpointList(HeadFlit* head_flit, Router* curr_router);

      // Private Variables
      volatile float _frequency;
      
      string _emesh_network;

      // Topology Parameters
      SInt32 _emesh_width;
      SInt32 _emesh_height;
};
