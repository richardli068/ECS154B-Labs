#include <vector>
#include <map>

#include "cache.hh"
#include "tag_array.hh"
#include "sram_array.hh"

class SetAssociativeCache: public Cache
{
public:
  /**
   * @param size : the *total* size of the cache in bytes
   * @param memory : the memory that is below this cache
   * @param processor : the processor this cache is connected to
   * @param ways : number of ways in this set associative cache. If the number
   *        of ways cannot be realized, this will cause an error
   */
  SetAssociativeCache(int64_t size, Memory& memory, Processor& processor,
                      int ways);
  
  /**
   * Destructor
   */
  ~SetAssociativeCache() override;
  
  /**
   * Called when the processors sends load or store request.
   * All requests can be assummed to be naturally aligned (e.g., a 4 byte
   * request will be aligned to a 4 byte boundary)
   *
   * @param address of the request
   * @param size in bytes of the request.
   * @param data is non-null, then this is a store request.
   * @param request_id the id that must be used when replying to this request
   *
   * @return true if the request can be received, false if the cache is
   *         blocked and the request must be retried later.
   */
  virtual bool receiveRequest(uint64_t address, int size, const uint8_t* data,
                              int request_id) override;
  
  /**
   * Called when memory id finished processing a request.
   * Data will always be the length of memory.getLineSize()
   *
   * @param request_id is the id assigned to this request in sendMemRequest
   * @param data is the data from memory (length of data is line length)
   *        NOTE: This pointer will be invalid when this function returns.
   */
  virtual void receiveMemResponse(int request_id, const uint8_t* data)
  override;
  
protected:
  /// Put any code you want here.
  struct MSHR {
    /// This is the current request_id that is blocking the cache.
    int savedId;
    
    /// The address for the blocking request.
    uint64_t savedAddr;
    
    /// This is the size of the original request. Needed for writes.
    int savedSize;
    
    int savedInsertIndex;
    
    /// This is the data that will be written after a miss
    const uint8_t* savedData;
    
    bool operator==(MSHR rhs) const
    {
      if (savedId == rhs.savedId && savedAddr == rhs.savedAddr
          && savedInsertIndex == rhs.savedInsertIndex
          && savedSize == rhs.savedSize) return true;
      else return false;
    }
  };
  
  enum State {
    Invalid=0,
    Valid=1,
    Invalid2=2,
    Dirty=3 // Dirty implies valid
  };
  uint64_t tagBits;
  uint64_t setMask;
  std::vector<TagArray*> tagArrayVec;
  std::vector<SRAMArray*> dataArrayVec;
  std::map<uint64_t, int> tagMap;
  int way;
  bool blocked;
  const MSHR mshrVoid;
  
  bool hit(uint64_t addr);
  int hitArray(uint64_t addr);
  bool dirty(uint64_t addr, int index);
  uint64_t getTag(uint64_t addr);
  uint64_t getSet(uint64_t addr);
  uint64_t getBlockOffset(uint64_t addr);
  
  void clearMSHR();
  void setMSHR(const MSHR mshr);
  MSHR getMSHR();
  
private:
  MSHR mshr;
};
