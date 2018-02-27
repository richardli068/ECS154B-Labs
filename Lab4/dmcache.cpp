//
//  dmcache.cpp
//  dmcache
//
//  Created by Richard Li on 11/18/17.
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
  short line;
  short offset;
  Memory(): tag(-1), line(0), offset(0){}
  Memory(unsigned int address){
    tag = address >> 8;
    line = (address & 0xF8) >> 3;
    offset = address & 0x7;
  }
  Memory operator=(Memory rhs) {
    tag = rhs.tag;
    line = rhs.line;
    offset = rhs.offset;
    return *this;
  }
  friend ostream& operator<<(ostream& ofs, const Memory& mm) {
    ofs << mm.tag << ' ' << mm.line << ' ' << mm.offset;
    return ofs;
  }
}; // class Memory

class CacheLine {
public:
  short dirty;
  short tag;
  short data[8];
  CacheLine(): tag(0), dirty(0) {
    for(int i = 0; i < 8; i++)
      data[i] = 0;
  }
  friend ostream& operator<<(ostream& ofs, const CacheLine& cl) {
    ofs << "dirty: " << cl.dirty << " tag: " << cl.tag << " data: " << cl.data;
    return ofs;
  }
}; // class CacheLine

int main(int argc, const char * argv[]) {
  short mainMemory[8192][8];
  short dirtyPrint;
  ifstream ifs(argv[1]);
  ofstream ofs("dm-out.txt");
  vector<CacheLine> cacheLines;
  for(int i = 0; i < 32; i++)
    cacheLines.push_back(CacheLine());
  unsigned int address, RW, data;
  while(ifs >> hex >> address >> RW >>data)
  {
    Memory memAdd = Memory(address);
    if(RW == 0x00) // read
    {
      if(cacheLines[memAdd.line].tag == memAdd.tag) // hit
      {
        ofs << setfill('0') << setw(2) << hex << uppercase
        << cacheLines[memAdd.line].data[memAdd.offset]
        << " 1 " << cacheLines[memAdd.line].dirty << endl;
        dirtyPrint = cacheLines[memAdd.line].dirty;
      }
      else // miss
      {
        if(cacheLines[memAdd.line].dirty == 1) // dirty
        {
          for(int i = 0; i < 8; i++)
          {
            mainMemory[(cacheLines[memAdd.line].tag << 5) + memAdd.line][i] =
            cacheLines[memAdd.line].data[i];
          }
        }
        for(int i = 0; i < 8; i++)
        {
          cacheLines[memAdd.line].data[i] =
          mainMemory[(memAdd.tag << 5) + memAdd.line][i];
        }
        cacheLines[memAdd.line].tag = memAdd.tag;
        dirtyPrint = cacheLines[memAdd.line].dirty;
        cacheLines[memAdd.line].dirty = 0;
        ofs << setfill('0') << setw(2) << hex << uppercase
        << cacheLines[memAdd.line].data[memAdd.offset]
        << " 0 " << dirtyPrint << endl;
      }
      short dataPrint = cacheLines[memAdd.line].data[memAdd.offset];
      short i = 0;
    }
    else if(RW == 0xFF) // write
    {
      if(cacheLines[memAdd.line].tag == memAdd.tag) // hit
      {
        cacheLines[memAdd.line].data[memAdd.offset] = data;
      }
      else // miss
      {
        if(cacheLines[memAdd.line].dirty == 1) // dirty
        {
          for(int i = 0; i < 8; i++)
          {
            mainMemory[(cacheLines[memAdd.line].tag << 5) + memAdd.line][i] =
            cacheLines[memAdd.line].data[i];
          }
        }
        for(int i = 0; i < 8; i++)
        {
          cacheLines[memAdd.line].data[i] =
          mainMemory[(memAdd.tag << 5) + memAdd.line][i];
        }
        cacheLines[memAdd.line].data[memAdd.offset] = data;
        cacheLines[memAdd.line].tag = memAdd.tag;
      }
      cacheLines[memAdd.line].dirty = 1;
    }
  }
  return 0;
}
