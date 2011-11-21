#pragma once

#include "directory_entry_limited.h"

class DirectoryEntryLimitedBroadcast : public DirectoryEntryLimited
{
public:
   DirectoryEntryLimitedBroadcast(SInt32 max_hw_sharers);
   ~DirectoryEntryLimitedBroadcast();
   
   bool addSharer(core_id_t sharer_id);
   void removeSharer(core_id_t sharer_id, bool reply_expected);
   
   bool getSharersList(vector<core_id_t>& sharers_list);
   SInt32 getNumSharers();

   UInt32 getLatency();

private:
   bool _global_enabled;
   UInt32 _num_sharers;
};
