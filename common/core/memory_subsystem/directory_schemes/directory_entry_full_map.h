#pragma once

#include "directory_entry.h"
#include "bit_vector.h"
#include "random.h"

class DirectoryEntryFullMap : public DirectoryEntry
{
public:
   DirectoryEntryFullMap(SInt32 max_hw_sharers);
   ~DirectoryEntryFullMap();
   
   bool hasSharer(core_id_t sharer_id);
   bool addSharer(core_id_t sharer_id);
   void removeSharer(core_id_t sharer_id, bool reply_expected);

   core_id_t getOwner();
   void setOwner(core_id_t owner_id);

   bool getSharersList(vector<core_id_t>& sharers_list);
   core_id_t getOneSharer();
   SInt32 getNumSharers();

   UInt32 getLatency();

private:
   BitVector* _sharers;
   Random _rand_num;
};
