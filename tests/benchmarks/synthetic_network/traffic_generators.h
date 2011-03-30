#pragma once

NetworkTrafficType parseTrafficPattern(string traffic_pattern);
void uniformRandomTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void bitComplementTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void shuffleTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void transposeTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void tornadoTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);
void nearestNeighborTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec);

// Mesh topology related
void computeEMeshTopologyParams(int num_cores, int& mesh_width, int& mesh_height);
void computeEMeshPosition(int core_id, int& sx, int& sy, int mesh_width);
int computeCoreId(int sx, int sy, int mesh_width);

NetworkTrafficType parseTrafficPattern(string traffic_pattern)
{
   if (traffic_pattern == "uniform_random")
      return UNIFORM_RANDOM;
   else if (traffic_pattern == "bit_complement")
      return BIT_COMPLEMENT;
   else if (traffic_pattern == "shuffle")
      return SHUFFLE;
   else if (traffic_pattern == "transpose")
      return TRANSPOSE;
   else if (traffic_pattern == "tornado")
      return TORNADO;
   else if (traffic_pattern == "nearest_neighbor")
      return NEAREST_NEIGHBOR;
   else
   {
      fprintf(stderr, "** ERROR **\n");
      fprintf(stderr, "Unrecognized Network Traffic Pattern Type (Use uniform_random, bit_complement, shuffle, transpose, tornado, nearest_neighbor)\n");
      exit(-1);
   }
}

void uniformRandomTrafficGenerator(int core_id, vector<int>& send_vec, vector<int>& receive_vec)
{
   // Generate Random Numbers using Linear Congruential Generator
   int send_matrix[_num_cores][_num_cores];
   int receive_matrix[_num_cores][_num_cores];

   send_matrix[0][0] = _num_cores / 2; // Initial seed
   receive_matrix[0][send_matrix[0][0]] = 0;
   for (int i = 0; i < _num_cores; i++) // Time Slot
   {
      if (i != 0)
      {
         send_matrix[i][0] = send_matrix[i-1][1];
         receive_matrix[i][send_matrix[i][0]] = 0;
      }
      for (int j = 1; j < _num_cores; j++) // Sender
      {
         send_matrix[i][j] = (13 * send_matrix[i][j-1] + 5) % _num_cores;
         receive_matrix[i][send_matrix[i][j]] = j;
      }
   }

   // Check the validity of the random numbers
   for (int i = 0; i < _num_cores; i++) // Time Slot
   {
      vector<bool> bits(_num_cores, false);
      for (int j = 0; j < _num_cores; j++) // Sender
      {
         bits[send_matrix[i][j]] = true;
      }
      for (int j = 0; j < _num_cores; j++)
      {
         assert(bits[j]);
      }
   }

   for (int j = 0; j < _num_cores; j++) // Sender
   {
      vector<bool> bits(_num_cores, false);
      for (int i = 0; i < _num_cores; i++) // Time Slot
      {
         bits[send_matrix[i][j]] = true;
      }
      for (int i = 0; i < _num_cores; i++)
      {
         assert(bits[i]);
      }
   }

   for (SInt32 i = 0; i < _num_cores; i++)
   {
      send_vec.push_back(send_matrix[i][core_id]);
      receive_vec.push_back(receive_matrix[i][core_id]);
   }
}

void bitComplementTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   assert(isPower2(_num_cores));
   int mask = _num_cores-1;
   int dst_core = (~core_id) & mask;
   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void shuffleTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   assert(isPower2(_num_cores));
   int mask = _num_cores-1;
   int nbits = floorLog2(_num_cores);
   int dst_core = ((core_id >> (nbits-1)) & 1) | ((core_id << 1) & mask);
   send_vec.push_back(dst_core); 
   receive_vec.push_back(dst_core); 
}

void transposeTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId(sy, sx, mesh_width);
   
   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void tornadoTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId((sx + mesh_width/2) % mesh_width, (sy + mesh_height/2) % mesh_height, mesh_width);

   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void nearestNeighborTrafficGenerator(int core_id, vector<core_id_t>& send_vec, vector<core_id_t>& receive_vec)
{
   int mesh_width, mesh_height;
   computeEMeshTopologyParams(_num_cores, mesh_width, mesh_height);
   int sx, sy;
   computeEMeshPosition(core_id, sx, sy, mesh_width);
   int dst_core = computeCoreId((sx+1) % mesh_width, (sy+1) % mesh_height, mesh_width);

   send_vec.push_back(dst_core);
   receive_vec.push_back(dst_core);
}

void computeEMeshTopologyParams(int num_cores, int& mesh_width, int& mesh_height)
{
   mesh_width = (int) sqrt((float) num_cores);
   mesh_height = (int) ceil(1.0 * num_cores / mesh_width);
   assert(num_cores == (mesh_width * mesh_height));
}

void computeEMeshPosition(int core_id, int& sx, int& sy, int mesh_width)
{
   sx = core_id % mesh_width;
   sy = core_id / mesh_width;
}

int computeCoreId(int sx, int sy, int mesh_width)
{
   return ((sy * mesh_width) + sx);
}
