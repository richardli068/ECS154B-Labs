
#include <cassert>

#include "set_assoc.hh"
#include "memory.hh"
#include "processor.hh"
#include "util.hh"

SetAssociativeCache::SetAssociativeCache(int64_t size, Memory& memory,
                                         Processor& processor, int ways)
: Cache(size, memory, processor),
tagBits(processor.getAddrSize() - log2int(size / memory.getLineSize() / ways) -
        memory.getLineBits()), setMask(size / memory.getLineSize() / ways - 1),
way(ways), blocked(false), mshr({-1,0,0,-1,nullptr})
{
  assert(ways > 0);
  for (int i = 0; i < ways; i++)
  {
    tagArrayVec.push_back(new TagArray((int)size / memory.getLineSize() / ways,
                                       2, (int) tagBits));
    dataArrayVec.push_back(new SRAMArray(size / memory.getLineSize() / ways,
                                         memory.getLineSize()));
  }
}

SetAssociativeCache::~SetAssociativeCache()
{
  for (int i = 0; i < tagArrayVec.size(); i++) delete tagArrayVec[i];
  for (int i = 0; i < dataArrayVec.size(); i++) delete dataArrayVec[i];
}

bool
SetAssociativeCache::receiveRequest(uint64_t address, int size,
                                    const uint8_t* data, int request_id)
{
  assert(size <= memory.getLineSize()); // within line size
  // within address range
  assert(address < ((uint64_t)1 << processor.getAddrSize()));
  assert((address &  (size - 1)) == 0); // naturally aligned
  
  if (blocked) {
    DPRINT("Cache is blocked!")
    return false;
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
      memcpy(&line[block_offset], data, size);
      sendResponse(request_id, nullptr);
      tagArrayVec[hitIndex]->setState(set, Dirty);
    }
    else // read
    {
      sendResponse(request_id, &line[block_offset]);
    }
  }
  else // miss
  {
    // random evict
    int evictIndex = rand() % way;
    mshr.savedEvictIndex = evictIndex;
    // since miss, randomly select a line in set.
    DPRINT("Miss in cache " << tagArrayVec[evictIndex]->getState(set));
    if (dirty(address))
    {
      DPRINT("Dirty, writing back");
      
      uint8_t* line = dataArrayVec[evictIndex]->getLine(set);
      uint64_t wb_address = tagArrayVec[evictIndex]->getTag(set) << (processor.getAddrSize() - tagBits);
      wb_address |= (set << memory.getLineBits());
      sendMemRequest(wb_address, memory.getLineSize(), line, -1);
    }
    tagArrayVec[evictIndex]->setState(set, Invalid);
    uint64_t block_address = address & ~(memory.getLineSize() -1);
    sendMemRequest(block_address, memory.getLineSize(), nullptr, 0);
    
    mshr.savedId = request_id;
    mshr.savedAddr = address;
    mshr.savedSize = size;
    mshr.savedData = data;
    
    blocked = true;
  }
  return true;
}

void
SetAssociativeCache::receiveMemResponse(int request_id, const uint8_t* data)
{
  assert(request_id == 0);
  assert(data);
  
  int set = (int) getSet(mshr.savedAddr);
  int evictIndex = mshr.savedEvictIndex;
  uint8_t* line = dataArrayVec[evictIndex]->getLine(set);
  memcpy(line, data, memory.getLineSize());
  
  assert(tagArrayVec[evictIndex]->getState(set) == Invalid);
  
  tagArrayVec[evictIndex]->setState(set, Valid);
  
  tagArrayVec[evictIndex]->setTag(set, getTag(mshr.savedAddr));
  
  int block_offset = (int) getBlockOffset(mshr.savedAddr);
  
  if (mshr.savedData) // write
  {
    memcpy(&line[block_offset], mshr.savedData, mshr.savedSize);
    sendResponse(mshr.savedId, nullptr);
    
    tagArrayVec[evictIndex]->setState(set, Dirty);
  }
  else // read
  {
    sendResponse(mshr.savedId, &line[block_offset]);
  }
  
  blocked = false;
  mshr.savedId = -1;
  mshr.savedAddr = 0;
  mshr.savedSize = 0;
  mshr.savedEvictIndex = -1;
  mshr.savedData = nullptr;
}

bool SetAssociativeCache::hit(uint64_t addr)
{
  int set = (int) getSet(addr);
  for (int i = 0; i < tagArrayVec.size(); i++)
  {
    State state = (State) tagArrayVec[i]->getState(set);
    uint64_t set_tag = tagArrayVec[i]->getTag(set);
    if ((state == Valid || state == Dirty)
        && set_tag == getTag(addr)) return true;
  }
  return false;
}

int SetAssociativeCache::hitArray(uint64_t addr)
{
  int index = 0;
  int set = (int) getSet(addr);
  for (int i = 0; i < tagArrayVec.size(); i++)
  {
    State state = (State) tagArrayVec[i]->getState(set);
    uint64_t set_tag = tagArrayVec[i]->getTag(set);
    if ((state == Valid || state == Dirty)
        && set_tag == getTag(addr)) index = i;
  }
  return index;
}

bool SetAssociativeCache::dirty(uint64_t addr)
{
  int set = (int) getSet(addr);
  for (int i = 0; i < tagArrayVec.size(); i++)
  {
    State state = (State) tagArrayVec[i]->getState(set);
    if (state == Dirty) return true;
  }
  // NOTE: if one TagArray is dirty, all tagArrays for this set is dirty.
  return false;
}

void SetAssociativeCache::setInvalid(int set)
{
  for (int i = 0; i < tagArrayVec.size(); i++)
  {
    tagArrayVec[i]->setState(set, Invalid);
  }
}

uint64_t SetAssociativeCache::getTag(uint64_t addr)
{
  return addr >> (processor.getAddrSize() - tagBits);
}

uint64_t SetAssociativeCache::getSet(uint64_t addr)
{
  return (addr >> memory.getLineBits()) & setMask;
}

uint64_t SetAssociativeCache::getBlockOffset(uint64_t addr)
{
  return addr & (memory.getLineSize() -  1);
}



