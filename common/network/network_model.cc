#include <cassert>
using namespace std;

#include "simulator.h"
#include "core_manager.h"
#include "network_types.h"
#include "network_model_magic.h"
#include "network_model_emesh_hop_counter.h"
#include "finite_buffer_network_model_emesh.h"
#include "finite_buffer_network_model_clos.h"
#include "finite_buffer_network_model_atac.h"
#include "finite_buffer_network_model_flip_atac.h"
#include "log.h"

#include "network.h"

NetworkModel::NetworkModel(Network *network, SInt32 network_id, bool is_finite_buffer):
   _network(network),
   _network_id(network_id),
   _is_finite_buffer(is_finite_buffer)
{
   if (network_id == 0)
      _network_name = "USER_1";
   else if (network_id == 1)
      _network_name = "USER_2";
   else if (network_id == 2)
      _network_name = "MEMORY_1";
   else if (network_id == 3)
      _network_name = "MEMORY_2";
   else if (network_id == 4)
      _network_name = "SYSTEM";
   else
      LOG_PRINT_ERROR("Unrecognized Network Num(%u)", network_id);

   _tile_width = Sim()->getCfg()->getFloat("general/tile_width");
}

NetworkModel*
NetworkModel::createModel(Network *net, SInt32 network_id, UInt32 model_type)
{
   switch (model_type)
   {
   case NETWORK_MAGIC:
      return new NetworkModelMagic(net, network_id);

   case NETWORK_EMESH_HOP_COUNTER:
      return new NetworkModelEMeshHopCounter(net, network_id);

   case FINITE_BUFFER_NETWORK_EMESH:
      return new FiniteBufferNetworkModelEMesh(net, network_id);

   case FINITE_BUFFER_NETWORK_ATAC:
      return new FiniteBufferNetworkModelAtac(net, network_id);

   case FINITE_BUFFER_NETWORK_CLOS:
	  return new FiniteBufferNetworkModelClos(net, network_id);
     
   case FINITE_BUFFER_NETWORK_FLIP_ATAC:
      return new FiniteBufferNetworkModelFlipAtac(net, network_id);
   
   default:
      LOG_PRINT_ERROR("Unrecognized Network Model(%u)", model_type);
      return NULL;
   }
}

UInt32 
NetworkModel::parseNetworkType(string str)
{
   if (str == "magic")
      return NETWORK_MAGIC;
   else if (str == "emesh_hop_counter")
      return NETWORK_EMESH_HOP_COUNTER;
   else if (str == "finite_buffer_emesh")
      return FINITE_BUFFER_NETWORK_EMESH;
   else if (str == "finite_buffer_atac")
      return FINITE_BUFFER_NETWORK_ATAC;
   else if(str == "finite_buffer_clos")
	  return FINITE_BUFFER_NETWORK_CLOS;
   else if (str == "finite_buffer_flip_atac")
     return FINITE_BUFFER_NETWORK_FLIP_ATAC;
   else
   {
      fprintf(stderr, "Unrecognized Network Type(%s)\n", str.c_str());
      exit(EXIT_FAILURE);
   }
}

pair<bool,SInt32>
NetworkModel::computeCoreCountConstraints(UInt32 network_type, SInt32 core_count)
{
   switch (network_type)
   {
      case NETWORK_MAGIC:
      case NETWORK_EMESH_HOP_COUNTER:
         return make_pair(false,core_count);

      case FINITE_BUFFER_NETWORK_EMESH:
         return FiniteBufferNetworkModelEMesh::computeCoreCountConstraints(core_count);

      case FINITE_BUFFER_NETWORK_ATAC:
         return FiniteBufferNetworkModelAtac::computeCoreCountConstraints(core_count);

      case FINITE_BUFFER_NETWORK_CLOS:
         return FiniteBufferNetworkModelClos::computeCoreCountConstraints(core_count);
			
      case FINITE_BUFFER_NETWORK_FLIP_ATAC:
         return FiniteBufferNetworkModelFlipAtac::computeCoreCountConstraints(core_count);
         
      default:
         fprintf(stderr, "Unrecognized network type(%u)\n", network_type);
         assert(false);
         return make_pair(false,-1);
   }
}

pair<bool, vector<core_id_t> > 
NetworkModel::computeMemoryControllerPositions(UInt32 network_type, SInt32 num_memory_controllers)
{
   switch(network_type)
   {
      case NETWORK_MAGIC:
      case NETWORK_EMESH_HOP_COUNTER:
         {
            SInt32 core_count = (SInt32) Config::getSingleton()->getTotalCores();
            SInt32 spacing_between_memory_controllers = core_count / num_memory_controllers;
            vector<core_id_t> core_list_with_memory_controllers;
            for (core_id_t i = 0; i < num_memory_controllers; i++)
            {
               assert((i*spacing_between_memory_controllers) < core_count);
               core_list_with_memory_controllers.push_back(i * spacing_between_memory_controllers);
            }
            
            return make_pair(false, core_list_with_memory_controllers);
         }

      case FINITE_BUFFER_NETWORK_EMESH:
         return FiniteBufferNetworkModelEMesh::computeMemoryControllerPositions(num_memory_controllers);
      
      case FINITE_BUFFER_NETWORK_ATAC:
         return FiniteBufferNetworkModelAtac::computeMemoryControllerPositions(num_memory_controllers);

      case FINITE_BUFFER_NETWORK_CLOS:
            return FiniteBufferNetworkModelClos::computeMemoryControllerPositions(num_memory_controllers);

      case FINITE_BUFFER_NETWORK_FLIP_ATAC:
         return FiniteBufferNetworkModelFlipAtac::computeMemoryControllerPositions(num_memory_controllers); 
         
      default:
         LOG_PRINT_ERROR("Unrecognized network type(%u)", network_type);
         return make_pair(false, vector<core_id_t>());
   }
}
