#pragma once

#include "directory_entry.h"
#include "random.h"

class DirectoryEntryLimited : public DirectoryEntry
{
public:
   DirectoryEntryLimited(SInt32 max_hw_sharers);
   ~DirectoryEntryLimited();

   bool hasSharer(core_id_t sharer_id);
   bool addSharer(core_id_t sharer_id);
   void removeSharer(core_id_t sharer_id);

   bool getSharersList(vector<core_id_t>& sharers);
   core_id_t getOneSharer();
   SInt32 getNumSharers();

protected:
   vector<SInt16> _sharers;
   SInt32 _num_tracked_sharers;
   static const SInt16 INVALID_SHARER = 0xffff;

private:
   Random _rand_num;
};
