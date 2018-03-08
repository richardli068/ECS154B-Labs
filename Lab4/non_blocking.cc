
#include <cassert>
#include <cstring>
#include <ctime>

#include "non_blocking.hh"
#include "util.hh"
#include "memory.hh"
#include "processor.hh"

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
        DPRINT("NB Cache is blocked!")
        return false;
      }

      if(isFound)
      {
        DPRINT("NB Cache already found entry")
        return true;
      }
    }


    int set = (int) getSet(address);

    if(hit(address))
    {
      DPRINT("Hit in cache");
      int hitIndex = hitArray(address);
      uint8_t* line = dataArrayVec[hitIndex]->getLine(set);
      int block_offset = (int) getBlockOffset(address);

      // write
      if (data)
      {
        std::memcpy(&line[block_offset], data, size);
        sendResponse(request_id, nullptr);
        tagArrayVec[hitIndex]->setState(set, Dirty);
        tagMap[set]++;
      }
      else // read
      {
        sendResponse(request_id, &line[block_offset]);
      }
    }
    else // miss
    {
      int insertIndex;
      if (tagMap[set] < way) // set still has space
      {
        insertIndex = tagMap[set];
        DPRINT("Miss in cache " << tagArrayVec[insertIndex]->getState(set));
        tagMap[set]++;
      }
      else // need to evict
      {
        // random evict
        srand((unsigned int) time(NULL));
        insertIndex = rand() % way;

        DPRINT("Miss in cache " << tagArrayVec[insertIndex]->getState(set));
        if (dirty(address, insertIndex))
        {
          DPRINT("Dirty, writing back");

          uint8_t* line = dataArrayVec[insertIndex]->getLine(set);
          uint64_t wb_address = tagArrayVec[insertIndex]->getTag(set) <<
                                      (processor.getAddrSize() - tagBits);
          wb_address |= (set << memory.getLineBits());
          sendMemRequest(wb_address, memory.getLineSize(), line, -1);
        }
      }
      tagArrayVec[insertIndex]->setState(set, Invalid);
      uint64_t block_address = address & ~(memory.getLineSize() -1);
      sendMemRequest(block_address, memory.getLineSize(), nullptr, 0);

      MSHR mshr;

      mshr.savedId = request_id;
      mshr.savedAddr = address;
      mshr.savedSize = size;
      mshr.savedInsertIndex = insertIndex;
      mshr.savedData = data;

      lookup_table.push_back(mshr);

      SetAssociativeCache::clearMSHR();
    }

    return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
  assert(request_id == 0);
  assert(data);
DPRINT("receiveMemResponse")

  MSHR first_entry = lookup_table[0];

  for (auto lt_it = lookup_table.begin(); lt_it != lookup_table.end(); ++lt_it)
  {
    if(first_entry.savedId == lt_it->savedId)
    {
      //We found it in the lookup table
      DPRINT("receiveMemResponse ID MATCH")
      int set = (int) getSet(lt_it->savedAddr);
      int insertIndex = lt_it->savedInsertIndex;
      uint8_t* line = dataArrayVec[insertIndex]->getLine(set);
      std::memcpy(line, data, memory.getLineSize());

      assert(tagArrayVec[insertIndex]->getState(set) == Invalid);

      tagArrayVec[insertIndex]->setState(set, Valid);

      tagArrayVec[insertIndex]->setTag(set, getTag(lt_it->savedAddr));

      int block_offset = (int) getBlockOffset(lt_it->savedAddr);

      if (lt_it->savedData) // write
      {
        memcpy(&line[block_offset], lt_it->savedData, lt_it->savedSize);
        sendResponse(lt_it->savedId, nullptr);
        tagArrayVec[insertIndex]->setState(set, Dirty);
      }
      else // read
      {
        // Invalid implies not Dirty
        sendResponse(lt_it->savedId, &line[block_offset]);
      }

      lookup_table.erase(lt_it);
    }
  }
}
