/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <string>
using namespace std;

#include "cache.h"
ulong protocol;
int main(int argc, char *argv[])
{
    
    ifstream fin;
    FILE * pFile;

    if(argv[1] == NULL){
         printf("input format: ");
         printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
         exit(0);
        }

    ulong cache_size     = atoi(argv[1]);
    ulong cache_assoc    = atoi(argv[2]);
    ulong blk_size       = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    protocol       = atoi(argv[5]); /* 0:MSI 1:MSI BusUpgr 2:MESI 3:MESI Snoop FIlter */
    char *fname        = (char *) malloc(20);
    fname              = argv[6];
    string protocol_name[4] = {"MSI", "MSI BusUpgr", "MESI", "MESI Filter"};
    printf("===== 506 Coherence Simulator Configuration =====\n");
    printf("L1_SIZE: %ld\n", cache_size);
    printf("L1_ASSOC: %ld\n", cache_assoc);
    printf("L1_BLOCKSIZE: %ld\n", blk_size);
    printf("NUMBER OF PROCESSORS: %ld\n", num_processors);
    printf("COHERENCE PROTOCOL: %s\n", protocol_name[protocol].c_str());
    printf("TRACE FILE: %s\n", fname);
    // print out simulator configuration here
    
    // Using pointers so that we can use inheritance */
    Cache** cacheArray = (Cache **) malloc(num_processors * sizeof(Cache));
    for(ulong i = 0; i < num_processors; i++) {
        if(protocol == 0) {
            cacheArray[i] = new MSI_Cache(cache_size, cache_assoc, blk_size);
        } else if (protocol == 1) {
            cacheArray[i] = new MSI_BusUpgr_Cache(cache_size, cache_assoc, blk_size);
        } else if (protocol == 2) {
            cacheArray[i] = new MESI_Cache(cache_size, cache_assoc, blk_size);
        } else if (protocol == 3) {
            cacheArray[i] = new MESI_Snoop_Filter_Cache(cache_size, cache_assoc, blk_size);
        } else {
            printf("Invalid protocol\n");
            exit(0);
        }
    }

    pFile = fopen (fname,"r");
    if(pFile == 0)
    {   
        printf("Trace file problem\n");
        exit(0);
    }
    
    ulong proc;
    char op;
    ulong addr;

    int line = 1;
    while(fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF)
    {
#ifdef _DEBUG
        printf("%d\n", line);
#endif
        // propagate request down through memory hierarchy
        // by calling cachesArray[processor#]->Access(...)
        busRequestType broadcastBusReq = BUS_REQ_MAX;
        if(proc < num_processors) {
            broadcastBusReq = cacheArray[proc]->Access(addr, op);
        } else {
            printf("Invalid processor number");
        }

        bool LineStatus = false;
        bool FlushOptCheck = false;
        for(int i=0;i<(int)num_processors;i++) {
            bool tempLineStatus = false;
            busRequestType tempBusReq = BUS_REQ_MAX;
            if(i != (int)proc) {
                tempBusReq = cacheArray[i]->snoop(addr,broadcastBusReq,tempLineStatus);
            }
            if(protocol >= 2)
            {
                LineStatus |= tempLineStatus;
                if(tempBusReq == BUS_REQ_FLUSH)
                {
                    FlushOptCheck = true;
                }
            }
        }

        if(protocol >= 2)
        {
            if(!LineStatus && (op == 'r') && (broadcastBusReq == BUS_REQ_READ))
            {
                cacheLine *line = cacheArray[proc]->findLine(addr);
                line->setFlags(STATE_EXCLUSIVE);
            }
            if(FlushOptCheck)
            {
                cacheArray[proc]->ct_cache_to_cache_transfers++;
            }
            if(!FlushOptCheck && ((broadcastBusReq == BUS_REQ_READ) || (broadcastBusReq == BUS_REQ_READX)))
            {
                cacheArray[proc]->ct_memory_transactions++;
            }
        }
        line++;
    }

    fclose(pFile);

    //********************************//
    //print out all caches' statistics //
    //********************************//
    for(int i=0;i<(int)num_processors;i++) {
        printf("============ Simulation results (Cache %d) ============\n",i);
        cacheArray[i]->printStats();
        if(protocol == 3)
        {
            cout << "14. number of useful snoops: " << ((MESI_Snoop_Filter_Cache *)cacheArray[i])->ct_snoop_filter_useful << endl;
            cout << "15. number of wasted snoops: " << ((MESI_Snoop_Filter_Cache *)cacheArray[i])->ct_snoop_filter_wasted << endl;
            cout << "16. number of filtered snoops: " << ((MESI_Snoop_Filter_Cache *)cacheArray[i])->ct_snoop_filter_filtered << endl;
        }
    }
    
}
