#pragma once

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelEMesh : public FiniteBufferNetworkModel
{
   public:
      FiniteBufferNetworkModelEMesh(Network* net, SInt32 network_id, bool broadcast_enabled);
      ~FiniteBufferNetworkModelEMesh();
  
      volatile float getFrequency() { return _frequency; }

      // Output Summary 
      void outputSummary(ostream &out);

      static pair<bool,SInt32> computeCoreCountConstraints(SInt32 core_count);
      static pair<bool,vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);
      static pair<bool,vector<Config::CoreList> > computeProcessToCoreMapping();
   
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
      SInt32 computeEMeshDistance(core_id_t sender, core_id_t receiver);

      // Static Functions
      static void computeEMeshTopologyParameters(SInt32& emesh_width, SInt32& emesh_height);

      // Main Routing Function
      void computeOutputEndpointList(Flit* head_flit, Router* curr_router);
      // Compute Unloaded Delay
      UInt64 computeUnloadedDelay(core_id_t sender, core_id_t receiver, SInt32 num_flits);

      // Private Variables
      volatile float _frequency;
      
      string _emesh_network;

      // Topology Parameters
      SInt32 _emesh_width;
      SInt32 _emesh_height;
      // Delay Parameters
      SInt32 _router_data_pipeline_delay;
      SInt32 _link_delay;
};
