#include "directory_entry_full_map.h"

using namespace std;

DirectoryEntryFullMap::DirectoryEntryFullMap(SInt32 max_hw_sharers)
   : DirectoryEntry(max_hw_sharers)
{
   _sharers = new BitVector(_max_hw_sharers);
}

DirectoryEntryFullMap::~DirectoryEntryFullMap()
{
   delete _sharers;
}

bool
DirectoryEntryFullMap::hasSharer(core_id_t sharer_id)
{
   return _sharers->at(sharer_id);
}

// Return value says whether the sharer was successfully added
//              'True' if it was successfully added
//              'False' if there will be an eviction before adding
bool
DirectoryEntryFullMap::addSharer(core_id_t sharer_id)
{
   assert(!_sharers->at(sharer_id));
   _sharers->set(sharer_id);
   return true;;
}

void
DirectoryEntryFullMap::removeSharer(core_id_t sharer_id, bool reply_expected)
{
   assert(!reply_expected);

   assert(_sharers->at(sharer_id));
   _sharers->clear(sharer_id);
}

// Return a pair:
// val.first :- 'True' if all cores are sharers
//              'False' if NOT all cores are sharers
// val.second :- A list of tracked sharers
bool
DirectoryEntryFullMap::getSharersList(vector<core_id_t>& sharers_list)
{
   sharers_list.resize(_sharers->size());

   _sharers->resetFind();

   core_id_t new_sharer = -1;
   SInt32 i = 0;
   while ((new_sharer = _sharers->find()) != -1)
   {
      sharers_list[i] = new_sharer;
      i++;
      assert (i <= (core_id_t) _sharers->size());
   }

   return false;
}

core_id_t
DirectoryEntryFullMap::getOneSharer()
{
   vector<core_id_t> sharers_list;
   getSharersList(sharers_list);

   SInt32 index = _rand_num.next(sharers_list.size());
   return sharers_list[index];
}

SInt32
DirectoryEntryFullMap::getNumSharers()
{
   return _sharers->size();
}

UInt32
DirectoryEntryFullMap::getLatency()
{
   return 0;
}
