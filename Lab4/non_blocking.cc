
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
/*
Algorithm idea:

if(isMiss)
{
  If(NOT in lookup_table)
  {
    if(lookup_table has reached max size)
    {
      return false
    }
  }
}

SetAssociativeCache::receiveRequest();
blocked = false; // Manually reset blocking bit

return true
*/
    bool isFound = false;
    bool isMiss = !hit(address);

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

      if(!isFound)
      {
        if(lookup_table.size() >= max_mshr_requests)
        {
          return false;
        }
      }
    }

    SetAssociativeCache::receiveRequest(address, size, data, request_id);
    blocked = false;

    if(!isFound && isMiss)
    {
      lookup_table.push_back(getMSHR());

      clearMSHR();
    }

    return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
/*
Algorithm idea:

if(request_id is in lookup_table)
{

  SetAssociativeCache::receiveMemResponse();

}
*/

  for (auto lt_it = lookup_table.begin(); lt_it != lookup_table.end();)
  {
    if(request_id == lt_it->savedId)
    {
      //We found it in the lookup table
      setMSHR(*lt_it);
      SetAssociativeCache::receiveMemResponse(request_id, data);

      lookup_table.erase(lt_it);

      return;
    }
  }
}
