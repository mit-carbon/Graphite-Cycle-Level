#pragma once

#include "finite_buffer_network_model.h"

class FiniteBufferNetworkModelEMesh : public FiniteBufferNetworkModel
{
   public:
      FiniteBufferNetworkModelEMesh(Network* net, SInt32 network_id);
      ~FiniteBufferNetworkModelEMesh();
  
      volatile float getFrequency() { return _frequency; }

      // Output Summary 
      void outputSummary(ostream &out);

      static pair<bool,SInt32> computeCoreCountConstraints(SInt32 core_count);
      static pair<bool,vector<core_id_t> > computeMemoryControllerPositions(SInt32 num_memory_controllers);
   
   private:
      enum NodeType
      {
         EMESH = 1 // Always start at 1
      };

      // Private Functions
      NetworkNode* createNetworkNode();

      void computeEMeshPosition(core_id_t core_id, SInt32& x, SInt32& y);
      core_id_t computeCoreId(SInt32 x, SInt32 y);
      SInt32 computeEMeshDistance(core_id_t sender, core_id_t receiver);

      // Static Functions
      static void computeEMeshTopologyParameters(SInt32& emesh_width, SInt32& emesh_height);

      // Main Routing Function
      void computeOutputEndpointList(HeadFlit* head_flit, NetworkNode* curr_network_node);

      // Event Count Summary
      void outputEventCountSummary(ostream& out);

      // Private Variables
      volatile float _frequency;
      
      string _emesh_network;

      // Topology Parameters
      SInt32 _emesh_width;
      SInt32 _emesh_height;
};
