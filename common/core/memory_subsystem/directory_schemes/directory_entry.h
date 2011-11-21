#pragma once

#include <vector>
using std::vector;

#include "fixed_types.h"
#include "directory_block_info.h"

class DirectoryEntry
{
public:
   DirectoryEntry(SInt32 max_hw_sharers);
   virtual ~DirectoryEntry();

   DirectoryBlockInfo* getDirectoryBlockInfo();

   virtual bool hasSharer(core_id_t sharer_id) = 0;
   virtual bool addSharer(core_id_t sharer_id) = 0;
   virtual void removeSharer(core_id_t sharer_id, bool reply_expected = false) = 0;

   core_id_t getOwner();
   void setOwner(core_id_t owner_id);

   IntPtr getAddress() { return _address; }
   void setAddress(IntPtr address) { _address = address; }

   virtual bool getSharersList(vector<core_id_t>& sharers_list) = 0;
   virtual core_id_t getOneSharer() = 0;
   virtual SInt32 getNumSharers() = 0;

   virtual UInt32 getLatency() = 0;

protected:
   IntPtr _address;
   DirectoryBlockInfo* _directory_block_info;
   core_id_t _owner_id;
   SInt32 _max_hw_sharers;
};
