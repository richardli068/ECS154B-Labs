
#include <iostream>

#include "direct_mapped.hh"
#include "memory.hh"
#include "processor.hh"
#include "set_assoc.hh"

int main(int argc, char *argv[])
{
    Processor p;
    Memory m(8);
    p.setMemory(&m);
//    DirectMappedCache c(1 << 10, m, p);
 SetAssociativeCache c =  SetAssociativeCache(1 << 10, m, p, 2);

//    std::cout << "Running simulation" << std::endl;
//    TickedObject::runSimulation();
//    std::cout << "Simulation done" << std::endl;
//
//    std::cout << "Data size: ";
//    std::cout << ((float)SRAMArray::getTotalSize())/1024 << "KB" << std::endl;
//
//    std::cout << "Tag size: ";
//    std::cout << ((float)TagArray::getTotalSize())/1024 << "KB" << std::endl;

}
