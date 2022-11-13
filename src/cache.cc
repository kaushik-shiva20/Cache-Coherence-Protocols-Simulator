/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b )
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
  
   //*******************//
   //initialize your counters here//
   //*******************//
    miss_rate = 0;
    ct_cache_to_cache_transfers = 0;
    ct_memory_transactions = 0;
    ct_interventions = 0;
    ct_invalidations = 0;
    ct_flushes = 0;
    ct_BusRdX = 0;
    ct_BusUpgr = 0;

   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
         cache[i][j].invalidate();
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
busRequestType Cache::Access(ulong addr,uchar op)
{
    busRequestType busReq = BUS_REQ_MAX;
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/

   if (op == 'w') writes++;
   else reads++;
   return busReq;
}

busRequestType Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent)
{
    busRequestType busReqRet = BUS_REQ_MAX;
    return busReqRet;
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
   if(cache[i][j].isValid()) {
      if(cache[i][j].getTag() == tag)
      {
         pos = j; 
         break; 
      }
   }
   if(pos == assoc) {
      return NULL;
   }
   else {
      return &(cache[i][pos]); 
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{

   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getFlags() == STATE_MODIFIED) {
       ct_memory_transactions++;
      writeBack(addr);
   }

   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(STATE_INVALID);
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats()
{
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
    cout << "01. number of reads: " << reads << endl;
    cout << "02. number of read misses: " << readMisses << endl;
    cout << "03. number of writes: " << writes << endl;
    cout << "04. number of write misses: " << writeMisses << endl;
    miss_rate = ((float)(readMisses + writeMisses)) / ((float)(reads + writes)) * 100;
    cout << "05. total miss rate: " << std::setprecision(3) << miss_rate << "%" << endl;
    cout << "06. number of writebacks: " << writeBacks << endl;
    cout << "07. number of cache-to-cache transfers: " << ct_cache_to_cache_transfers << endl;
    cout << "08. number of memory transactions: " << ct_memory_transactions << endl;
    cout << "09. number of interventions: " << ct_interventions << endl;
    cout << "10. number of invalidations: " << ct_invalidations << endl;
    cout << "11. number of flushes: " << ct_flushes << endl;
    cout << "12. number of BusRdX: " << ct_BusRdX << endl;
    cout << "13. number of BusUpgr: " << ct_BusUpgr << endl;
}

//MSI protocol
MSI_Cache::MSI_Cache(int s,int a,int b ): Cache(s,a,b)
{
    //Add any MSI specific initialization here
}

//This function handles processor R/W requests and MSI bus requests
busRequestType MSI_Cache::Access(ulong addr,uchar op){
    //This function handles processor R/W requests
    Cache::Access(addr, op);

    cacheLine *line = findLine(addr);
    if (line == NULL)/*miss*/{
        if (op == 'w') writeMisses++;
        else readMisses++;
        cacheLine *newline = fillLine(addr);
        line = newline;
    } else {
        /**since it's a hit, update LRU and update dirty flag**/
        updateLRU(line);
    }

    //MSI processor request handling
    busRequestType broadcaseReq = BUS_REQ_MAX;
    if(line != NULL) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_READX;
                    ct_BusRdX++;
                } else {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_READ;
                }
                ct_memory_transactions++;
                break;
            case STATE_SHARED:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_READX;
                    ct_BusRdX++;
                    ct_memory_transactions++;
                }
                break;
            case STATE_MODIFIED:
                break;
        }
    }
    return broadcaseReq;
}

busRequestType MSI_Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent) {
    //MSI bus requests handling
    cacheLine *line = findLine(addr);
    busRequestType broadcaseReq = BUS_REQ_MAX;
    if(line != NULL && busReq != BUS_REQ_MAX) {

        switch (line->getFlags()) {
            case STATE_INVALID:
                break;
            case STATE_SHARED:
                if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                }
                break;
            case STATE_MODIFIED:
                if (busReq == BUS_REQ_READ) {
                    line->setFlags(STATE_SHARED);
                    ct_interventions++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }else if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }
                break;
        }
    }
    return broadcaseReq;
}

//MSI with bus upgrade protocol
MSI_BusUpgr_Cache::MSI_BusUpgr_Cache(int s,int a,int b ): Cache(s,a,b)
{
    //Add any MSI_BusUpgr specific initialization here
}

//This function handles processor R/W requests and MSI bus requests
busRequestType MSI_BusUpgr_Cache::Access(ulong addr,uchar op){
    //This function handles processor R/W requests
    Cache::Access(addr, op);

    //MSI with bus upgrade processor request handling
    cacheLine *line = findLine(addr);
    if (line == NULL)/*miss*/{
        if (op == 'w') writeMisses++;
        else readMisses++;
        cacheLine *newline = fillLine(addr);
        line = newline;
    } else {
        /**since it's a hit, update LRU and update dirty flag**/
        updateLRU(line);
    }

    busRequestType broadcaseReq = BUS_REQ_MAX;
    if(line != NULL) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_READX;
                    ct_BusRdX++;
                } else {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_READ;
                }
                ct_memory_transactions++;
                break;
            case STATE_SHARED:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_UPGRADE;
                    ct_BusUpgr++;
                }
                break;
            case STATE_MODIFIED:
                break;
        }
    }
    return broadcaseReq;

}

busRequestType MSI_BusUpgr_Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent) {
    //MSI with bus upgrade bus requests handling
    cacheLine *line = findLine(addr);
    busRequestType broadcaseReq = BUS_REQ_MAX;

    if(line != NULL && busReq != BUS_REQ_MAX) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                break;
            case STATE_SHARED:
                if (busReq == BUS_REQ_READX || busReq == BUS_REQ_UPGRADE) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                }
                if(busReq == BUS_REQ_UPGRADE)
                {
                   // ct_BusUpgr++;
                }
                break;
            case STATE_MODIFIED:
                if (busReq == BUS_REQ_READ) {
                    line->setFlags(STATE_SHARED);
                    ct_interventions++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }else if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }
                break;
        }
    }
    return broadcaseReq;
}

//MESI protocol
MESI_Cache::MESI_Cache(int s,int a,int b ): Cache(s,a,b)
{
    //Add any MESI specific initialization here
}


//This function handles processor R/W requests and MESI bus requests
busRequestType MESI_Cache::Access(ulong addr,uchar op){
    //This function handles processor R/W requests
    Cache::Access(addr, op);

    //MESI processor request handling
    cacheLine *line = findLine(addr);
    if (line == NULL)/*miss*/{
        if (op == 'w') writeMisses++;
        else readMisses++;
        cacheLine *newline = fillLine(addr);
        line = newline;
    } else {
        /**since it's a hit, update LRU and update dirty flag**/
        updateLRU(line);
    }

    busRequestType broadcaseReq = BUS_REQ_MAX;
    if(line != NULL) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_READX;
                    ct_BusRdX++;
                } else {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_READ;
                }
                //ct_memory_transactions++;
                break;
            case STATE_SHARED:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_UPGRADE;
                    ct_BusUpgr++;
                }
                break;
            case STATE_EXCLUSIVE:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                }
                break;
            case STATE_MODIFIED:
                break;
        }
    }
    return broadcaseReq;
}

busRequestType MESI_Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent) {
    //MESI with bus upgrade bus requests handling
    cacheLine *line = findLine(addr);
    busRequestType broadcaseReq = BUS_REQ_MAX;

    if(line != NULL && busReq != BUS_REQ_MAX) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                break;
            case STATE_SHARED:
                if (busReq == BUS_REQ_READX || busReq == BUS_REQ_UPGRADE) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                }
                if(busReq == BUS_REQ_READ || busReq == BUS_REQ_READX)
                {
                    broadcaseReq = BUS_REQ_FLUSH;
                }
                isLinePresent = true;
                break;
            case STATE_EXCLUSIVE:
                if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_invalidations++;
                }else if(busReq == BUS_REQ_READ)
                {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_interventions++;
                }
                isLinePresent = true;
            break;
            case STATE_MODIFIED:
                if (busReq == BUS_REQ_READ) {
                    line->setFlags(STATE_SHARED);
                    ct_interventions++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }else if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }
                isLinePresent = true;
                break;
        }
    }
    return broadcaseReq;
}

//MESI with Snoop filter protocol
MESI_Snoop_Filter_Cache::MESI_Snoop_Filter_Cache(int s,int a,int b ): Cache(s,a,b),SnoopFilter(1024,1,64)
{
    //Add any MESI_Snoop_Filter specific initialization here
    ct_snoop_filter_useful = 0;
    ct_snoop_filter_wasted = 0;
    ct_snoop_filter_filtered = 0;
}

//This function handles processor R/W requests and MESI bus requests
busRequestType MESI_Snoop_Filter_Cache::Access(ulong addr,uchar op){
    //This function handles processor R/W requests
    Cache::Access(addr, op);

    //MESI processor request handling
    cacheLine *line = findLine(addr);
    cacheLine * snoopLine = SnoopFilter.findLine(addr);
    if (line == NULL)/*miss*/{
        if (op == 'w') writeMisses++;
        else readMisses++;
        cacheLine *newline = fillLine(addr);
        line = newline;
    } else {
        /**since it's a hit, update LRU and update dirty flag**/
        updateLRU(line);
    }

    busRequestType broadcaseReq = BUS_REQ_MAX;
    if(line != NULL) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_READX;
                    ct_BusRdX++;
                    if(snoopLine != NULL)
                    {
                        snoopLine->setFlags(STATE_INVALID);
                    }
                } else {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_READ;
                    if(snoopLine != NULL)
                    {
                        snoopLine->setFlags(STATE_INVALID);
                    }
                }
                //ct_memory_transactions++;
                break;
            case STATE_SHARED:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                    broadcaseReq = BUS_REQ_UPGRADE;
                    ct_BusUpgr++;
                }
                break;
            case STATE_EXCLUSIVE:
                if (op == 'w') {
                    line->setFlags(STATE_MODIFIED);
                }
                break;
            case STATE_MODIFIED:
                break;
        }
    }
    return broadcaseReq;

}

busRequestType MESI_Snoop_Filter_Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent) {
    //MESI with bus upgrade bus requests handling
    cacheLine *line = findLine(addr);
    busRequestType broadcaseReq = BUS_REQ_MAX;

    cacheLine * snoopLine = SnoopFilter.findLine(addr);
    if(snoopLine != NULL)
    {
        //line is present in snoop filter so no need to handle bus request
        ct_snoop_filter_filtered++;
    }else
    {
        if(line == NULL)
        {
            //line not found in snoop filter and cache
            ct_snoop_filter_wasted++;
            SnoopFilter.fillLine(addr)->setFlags(STATE_MODIFIED);
        }else
        {
            //line not found in snoop filter but found in cache
            ct_snoop_filter_useful++;
        }
    }

    if(line != NULL && busReq != BUS_REQ_MAX) {
        switch (line->getFlags()) {
            case STATE_INVALID:
                break;
            case STATE_SHARED:
                if (busReq == BUS_REQ_READX || busReq == BUS_REQ_UPGRADE) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                    SnoopFilter.fillLine(addr)->setFlags(STATE_MODIFIED);;
                }
                if(busReq == BUS_REQ_READ || busReq == BUS_REQ_READX)
                {
                    broadcaseReq = BUS_REQ_FLUSH;
                }
                isLinePresent = true;
                break;
            case STATE_EXCLUSIVE:
                if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_invalidations++;
                    SnoopFilter.fillLine(addr)->setFlags(STATE_MODIFIED);;
                }else if(busReq == BUS_REQ_READ)
                {
                    line->setFlags(STATE_SHARED);
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_interventions++;
                }
                isLinePresent = true;
                break;
            case STATE_MODIFIED:
                if (busReq == BUS_REQ_READ) {
                    line->setFlags(STATE_SHARED);
                    ct_interventions++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                }else if (busReq == BUS_REQ_READX) {
                    line->setFlags(STATE_INVALID);
                    ct_invalidations++;
                    broadcaseReq = BUS_REQ_FLUSH;
                    ct_flushes++;
                    ct_memory_transactions++;
                    writeBacks++;
                    SnoopFilter.fillLine(addr)->setFlags(STATE_MODIFIED);;
                }
                isLinePresent = true;
                break;
        }
    }
    return broadcaseReq;
}

//MOESI protocol
MOESI_Cache::MOESI_Cache(int s,int a,int b ): Cache(s,a,b)
{
    //Add any MOESI specific initialization here
}

//This function handles processor R/W requests and MOESI bus requests
busRequestType MOESI_Cache::Access(ulong addr,uchar op){
    Cache::Access(addr, op);
    busRequestType busReq = BUS_REQ_MAX;
    return busReq;
}

busRequestType MOESI_Cache::snoop(ulong addr, busRequestType busReq, bool &isLinePresent) {
    //TODO: MOESI with snoop filter bus requests handling
    busRequestType busReqRet = BUS_REQ_MAX;
    return busReqRet;
}


