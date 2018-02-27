//
//  sacache.cpp
//  sacache
//
//  Created by Richard Li on 11/19/17.
//  Copyright © 2017 Richard Li. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

using namespace std;

class Memory {
public:
  short tag;
  short set;
  short offset;
  Memory(): tag(-1), set(0), offset(0){}
  Memory(unsigned int address){
    tag = address >> 6;
    set = (address & 0x3C) >> 2;
    offset = address & 0x3;
  }
  Memory operator=(Memory rhs) {
    tag = rhs.tag;
    set = rhs.set;
    offset = rhs.offset;
    return *this;
  }
  friend ostream& operator<<(ostream& ofs, const Memory& mm) {
    ofs << mm.tag << ' ' << mm.set << ' ' << mm.offset;
    return ofs;
  }
}; // class Memory

class CacheLine {
public:
  short dirty;
  short tag;
  short data[4];
  short age;
  CacheLine(): tag(-1), dirty(0), age(0) {
    for(int i = 0; i < 4; i++)
      data[i] = 0;
  }
  void setUnused() {
    *this = CacheLine();
  }
  bool isUnused() {
    if(dirty == 0 && tag == -1 && data[0] == 0 && data[1] == 0 && data[2] == 0
       && data[3] == 0 && age == 0)
      return true;
    else return false;
  }
  friend ostream& operator<<(ostream& ofs, const CacheLine& cl) {
    ofs << "dirty: " << cl.dirty << " tag: " << cl.tag << " data: " << cl.data;
    return ofs;
  }
}; // class CacheLine

class CacheSet {
public:
  CacheLine cl[8];
  short used;
  CacheSet(): used(0) {
    for(int i = 0; i < 8; i++)
      cl[i] = CacheLine();
  }
  bool isInSet(short tag) {
    for(int i = 0; i < 8; i++)
    {
      if(cl[i].tag == tag)
        return true;
    }
    return false;
  }
  short getLineIndex(short tag) {
    short index = -1;
    for(int i = 0; i < 8; i++)
    {
      if(cl[i].tag == tag)
        index = i;
    }
    return index;
  }
  short getLRU() {
    short index = 0;
    short oldest = 0;
    for(int i = 0; i < 8; i++)
    {
      if(cl[i].age > oldest)
      {
        index = i;
        oldest = cl[i].age;
      }
    }
    return index;
  }
  short getAvail() {
    short index = 7;
    for(short i = 0; i < 8; i++)
    {
      if(cl[i].isUnused() && i < index)
        index = i;
    }
    return index;
  }
  void older(short index) {
    for(int i = 0; i < 8; i++)
    {
      if(!cl[i].isUnused())
        cl[i].age++;
    }
    cl[index].age = 0;
  }
}; // class CacheSet

int main(int argc, const char * argv[]) {
  short mainMemory[16384][8];
  short dirtyPrint;
  ifstream ifs(argv[1]);
  ofstream ofs("sa-out.txt");
  vector<CacheSet> cacheSets;
  for(int i = 0; i < 16; i++)
    cacheSets.push_back(CacheSet());
  unsigned int address, RW, data;
  while(ifs >> hex >> address >> RW >> data)
  {
    Memory memAdd = Memory(address);
    if(memAdd.set == 0x6)
      int x = 0;
    if(RW == 0x00) // read
    {
      if(cacheSets[memAdd.set].isInSet(memAdd.tag)) // hit
      {
        short index = cacheSets[memAdd.set].getLineIndex(memAdd.tag);
        cacheSets[memAdd.set].older(index);
        ofs << setfill('0') << setw(2) << hex << uppercase <<
        cacheSets[memAdd.set].cl[index].data[memAdd.offset]
        << " 1 " << cacheSets[memAdd.set].cl[index].dirty << endl;
      }
      else // miss
      {
        short index;
        if(cacheSets[memAdd.set].used == 8) // full
        {
          index = cacheSets[memAdd.set].getLRU();
          cacheSets[memAdd.set].used--;
        }
        else // not full
          index = cacheSets[memAdd.set].getAvail();
        if(cacheSets[memAdd.set].cl[index].dirty == 1) // dirty
        {
          for(int i = 0; i < 4; i++)
          {
            mainMemory[(cacheSets[memAdd.set].cl[index].tag << 4) + memAdd.set][i] =
            cacheSets[memAdd.set].cl[index].data[i];
          }
        }
        for(int i = 0; i < 4; i++)
        {
          cacheSets[memAdd.set].cl[index].data[i] =
          mainMemory[(memAdd.tag << 4) + memAdd.set][i];
        }
        cacheSets[memAdd.set].used++;
        dirtyPrint = cacheSets[memAdd.set].cl[index].dirty;
        cacheSets[memAdd.set].cl[index].tag = memAdd.tag;
        cacheSets[memAdd.set].cl[index].dirty = 0;
        cacheSets[memAdd.set].older(index);
        ofs << setfill('0') << setw(2) << hex << uppercase
        << cacheSets[memAdd.set].cl[index].data[memAdd.offset]
        << " 0 " << dirtyPrint << endl;
      }
    }
    else if(RW == 0xFF) // write
    {
      if(cacheSets[memAdd.set].isInSet(memAdd.tag)) // hit
      {
        short index = cacheSets[memAdd.set].getLineIndex(memAdd.tag);
        cacheSets[memAdd.set].cl[index].tag = memAdd.tag;
        cacheSets[memAdd.set].cl[index].data[memAdd.offset] = data;
        cacheSets[memAdd.set].older(index);
        cacheSets[memAdd.set].cl[index].dirty = 1;
      }
      else // miss
      {
        short index;
        if(cacheSets[memAdd.set].used == 8) // full
        {
          index = cacheSets[memAdd.set].getLRU();
          cacheSets[memAdd.set].used--;
        }
        else // not full
          index = cacheSets[memAdd.set].getAvail();
        if(cacheSets[memAdd.set].cl[index].dirty == 1) // dirty
        {
          for(int i = 0; i < 4; i++)
          {
            mainMemory[(cacheSets[memAdd.set].cl[index].tag << 4) + memAdd.set][i] =
            cacheSets[memAdd.set].cl[index].data[i];
          }
        }
        for(int i = 0; i < 4; i++)
        {
          cacheSets[memAdd.set].cl[index].data[i] =
          mainMemory[(memAdd.tag << 4) + memAdd.set][i];
        }
        cacheSets[memAdd.set].used++;
        cacheSets[memAdd.set].cl[index].tag = memAdd.tag;
        cacheSets[memAdd.set].cl[index].data[memAdd.offset] = data;
        cacheSets[memAdd.set].older(index);
        cacheSets[memAdd.set].cl[index].dirty = 1;
      }
    }
    CacheSet test;
    test.cl[0] = CacheLine();
    test.cl[0].data[1] = 1;
    test.cl[0].age = 0;
    test.cl[0].dirty = 1;
    test.cl[0].tag = 1;
    test.cl[0].setUnused();
    int x = 0;
  }
  return 0;
}
