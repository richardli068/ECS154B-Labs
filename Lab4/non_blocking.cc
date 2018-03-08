
#include <cassert>

#include "non_blocking.hh"
#include "util.hh"

NonBlockingCache::NonBlockingCache(int64_t size, Memory& memory,
                                   Processor& processor, int ways, int mshrs)
    : SetAssociativeCache(size, memory, processor, ways)
{
    assert(mshrs > 0);

    max_mshr_requests = mshrs;
}

NonBlockingCache::~NonBlockingCache()
{

}

bool
NonBlockingCache::receiveRequest(uint64_t address, int size,
                                const uint8_t* data, int request_id)
{
    bool isFound = false;
    bool isMiss = !SetAssociativeCache::hit(address);

    if(isMiss)
    {
      for(auto table_entry : lookup_table)
      {
        if(address == table_entry.savedAddr)
        {
          //We found it in the lookup table
          isFound = true;
          break;
        }
      }

      if(!isFound && lookup_table.size() >= max_mshr_requests)
      {
        return false;
      }
    }

    SetAssociativeCache::receiveRequest(address, size, data, request_id);

    if(!isFound && isMiss)
    {
      lookup_table.push_back(SetAssociativeCache::getMSHR());

      SetAssociativeCache::clearMSHR();
      blocked = false;
    }

    return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
  for (auto lt_it = lookup_table.begin(); lt_it != lookup_table.end(); ++lt_it)
  {
    if(request_id == lt_it->savedId)
    {
      //We found it in the lookup table
      SetAssociativeCache::setMSHR(*lt_it);
      SetAssociativeCache::receiveMemResponse(request_id, data);

      lookup_table.erase(lt_it);

      return;
    }
  }
}
