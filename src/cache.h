/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>
#include <iomanip>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

extern ulong protocol;
/****add new states, based on the protocol****/
enum {
    STATE_INVALID = 0,
    STATE_MODIFIED,
    STATE_OWNED,
    STATE_EXCLUSIVE,
    STATE_SHARED,
    STATE_MAX
};


enum busRequestType{
    BUS_REQ_UPGRADE = 0,
    BUS_REQ_READ,
    BUS_REQ_READX,
    BUS_REQ_FLUSH,
    BUS_REQ_MAX
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq;
 
public:
   cacheLine()                { tag = 0; Flags = 0; }
   ulong getTag()             { return tag; }
   ulong getFlags()           { return Flags;}
   ulong getSeq()             { return seq; }
   void setSeq(ulong Seq)     { seq = Seq;}
   void setFlags(ulong flags) {  Flags = flags;}
   void setTag(ulong a)       { tag = a; }
   void invalidate()          { tag = 0; Flags = STATE_INVALID; } //useful function
   bool isValid()             { return ((Flags) != STATE_INVALID); }
};

class Cache
{
protected:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads,readMisses,writes,writeMisses,writeBacks;

   //******///
   //add coherence counters here///
   //******///


   cacheLine **cache;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
   
public:
    float miss_rate;
    ulong ct_cache_to_cache_transfers;
    ulong ct_memory_transactions;
    ulong ct_interventions;
    ulong ct_invalidations;
    ulong ct_flushes;
    ulong ct_BusRdX;
    ulong ct_BusUpgr;

    ulong currentCycle;  
     
    Cache(int,int,int);
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM()     {return readMisses;} 
   ulong getWM()     {return writeMisses;} 
   ulong getReads()  {return reads;}       
   ulong getWrites() {return writes;}
   ulong getWB()     {return writeBacks;}
   
   void writeBack(ulong) {writeBacks++;}
   virtual busRequestType Access(ulong,uchar);
   virtual busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
   void printStats();
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   //******///

};

class MSI_Cache: public Cache
{
public:
    busRequestType Access(ulong,uchar);
    busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
    MSI_Cache(int,int,int);

};

class MSI_BusUpgr_Cache: public Cache
{
public:
    busRequestType Access(ulong,uchar);
    busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
    MSI_BusUpgr_Cache(int,int,int);

};

class MESI_Cache: public Cache
{
protected:
    busRequestType Access(ulong,uchar);
    busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
public:
    MESI_Cache(int,int,int);

};

class MESI_Snoop_Filter_Cache: public Cache
{
public:
    Cache SnoopFilter;
    ulong ct_snoop_filter_useful;
    ulong ct_snoop_filter_wasted;
    ulong ct_snoop_filter_filtered;
    busRequestType Access(ulong,uchar);
    busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
    MESI_Snoop_Filter_Cache(int,int,int);

};

class MOESI_Cache: public Cache
{
public:
    busRequestType Access(ulong,uchar);
    busRequestType snoop(ulong addr, busRequestType busReq, bool &isLinePresent);
    MOESI_Cache(int,int,int);

};


#endif
