
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
  assert(size <= memory.getLineSize()); // within line size
  // within address range
  assert(address < ((uint64_t)1 << processor.getAddrSize()));
  assert((address &  (size - 1)) == 0); // naturally aligned
  
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
    if(lookup_table.size() >= max_mshr_requests)
    {
      DPRINT("MSHR full!")
      return false;
    }
    // always allocate in MSHR table
    MSHR mshr;
    mshr.savedId = request_id;
    mshr.savedAddr = address;
    mshr.savedSize = size;
    mshr.savedData = data;
    
    int insertIndex;
    if (tagMap[set] < way) // set still has space
    {
      insertIndex = tagMap[set];
      DPRINT("Miss in cache " << tagArrayVec[insertIndex]->getState(set));
      tagMap[set]++;
      mshr.savedInsertIndex = insertIndex;
    }
    else // need to evict
    {
      // random evict
      srand((unsigned int) time(NULL));
      insertIndex = rand() % way;
      mshr.savedInsertIndex = insertIndex;
      
      DPRINT("Miss in cache " << tagArrayVec[insertIndex]->getState(set));
      if (dirty(address, insertIndex))
      {
        DPRINT("Dirty, writing back");
        
        uint8_t* line = dataArrayVec[insertIndex]->getLine(set);
        uint64_t wb_address = tagArrayVec[insertIndex]->getTag(set) <<
        (processor.getAddrSize() - tagBits);
        wb_address |= (set << memory.getLineBits());
        sendMemRequest(wb_address, memory.getLineSize(), line, request_id);
      }
    }
    lookup_table.push_back(mshr);
    
    tagArrayVec[insertIndex]->setState(set, Invalid);
    uint64_t block_address = address & ~(memory.getLineSize() -1);
    sendMemRequest(block_address, memory.getLineSize(), nullptr, request_id);
  }
  return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
  //  assert(request_id == 0); not needed because valid request_id is required
  assert(data);
  
  std::vector<MSHR>::iterator lt_it;
  for (lt_it = lookup_table.begin(); lt_it!= lookup_table.end(); lt_it++)
  {
    if (lt_it->savedId == request_id) break;
  }
  
  int set = (int) getSet(lt_it->savedAddr);
  int insertIndex;
  uint8_t* line;
  
  if (!hit(lt_it->savedAddr)) // miss
  {
    insertIndex = lt_it->savedInsertIndex;
    line = dataArrayVec[insertIndex]->getLine(set);
    // previous entry in MSHR not resolve current request
    std::memcpy(line, data, memory.getLineSize());
    
//  assert(tagArrayVec[insertIndex]->getState(set) == Invalid);
//  not needed because MSHR could previously set this to Dirty and cause trouble
    
    tagArrayVec[insertIndex]->setState(set, Valid);
    
    tagArrayVec[insertIndex]->setTag(set, getTag(lt_it->savedAddr));
  }
  else // hit
  {
    insertIndex = hitArray(lt_it->savedAddr);
    line = dataArrayVec[insertIndex]->getLine(set);
    // if previous MSHR resolve current one, free 1 tag array
    tagMap[set]--;
  }
  
  int block_offset = (int) getBlockOffset(lt_it->savedAddr);
  if (lt_it->savedData) // write
  {
    std::memcpy(&line[block_offset], lt_it->savedData, lt_it->savedSize);
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
