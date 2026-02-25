/*
 * Copyright (c) 2012-2014 Pennsylvania State University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include
 *  the original NVMain paper in the citation list and mention the use
 *  of NVMain.
 *
 */

#include "SimInterface/Gem5Interface/Gem5Interface.h"
#include "Simulators/gem5/tmp/nvmain_mem_interface.hh"
#include "Utils/HookFactory.h"

#include "base/random.hh"
#include "base/statistics.hh"
#include "debug/NVMain.hh"
#include "debug/NVMainMin.hh"
#include "config/kvm_isa.hh"

using namespace NVM;
using namespace gem5::memory;

// This members are singleton values used to hold the main instance of
// NVMain and it's wake/sleep (i.e., timing/atomic) status. These are
// needed since NVMain assumes a contiguous address range while gem5
// ISAs generally do not. The multiple instances allow for the gem5
// AddrRanges to be used normally while this class remapped to NVMains
// contiguous region.
NVMainMemInterface *NVMainMemInterface::masterInstance = NULL;

NVMainMemInterface::NVMainMemInterface(const Params *p)
    : MemInterface(*p), clockEvent(this), respondEvent(this),
      drainManager(NULL), lat(p->atomic_latency),
      lat_var(p->atomic_variance), nvmain_atomic(p->atomic_mode),
      NVMainWarmUp(p->NVMainWarmUp)
{
    char *cfgparams;
    char *cfgvalues;
    char *cparam, *cvalue;

    char *saveptr1, *saveptr2;

    nextEventCycle = 0;

    m_nvmainPtr = NULL;
    m_nacked_requests = false;

    m_nvmainConfigPath = p->config;

    m_nvmainConfig = new Config( );

    m_nvmainConfig->Read( m_nvmainConfigPath );
    std::cout << "NVMainControl: Reading NVMain config file: " << m_nvmainConfigPath << "." << std::endl;

    clock = clockPeriod( );

    m_avgAtomicLatency = 100.0f;
    m_numAtomicAccesses = 0;

    retryRead = false;
    retryWrite = false;
    retryResp = false;
    m_requests_outstanding = 0;

    /*
     * Modified by Tao @ 01/22/2013
     * multiple parameters can be manually specified
     * please separate the parameters by comma ","
     * For example,
     *    configparams = tRCD,tCAS,tRP
     *    configvalues = 8,8,8
     */
    cfgparams = (char *)p->configparams.c_str();
    cfgvalues = (char *)p->configvalues.c_str();

    for( cparam = strtok_r( cfgparams, ",", &saveptr1 ), cvalue = strtok_r( cfgvalues, ",", &saveptr2 )
           ; (cparam && cvalue) ; cparam = strtok_r( NULL, ",", &saveptr1 ), cvalue = strtok_r( NULL, ",", &saveptr2) )
    {
        std::cout << "NVMain: Overriding parameter `" << cparam << "' with `" << cvalue << "'" << std::endl;
        m_nvmainConfig->SetValue( cparam, cvalue );
    }

   BusWidth = m_nvmainConfig->GetValue( "BusWidth" );
   tBURST = m_nvmainConfig->GetValue( "tBURST" );
   RATE = m_nvmainConfig->GetValue( "RATE" );

   lastWakeup = curTick();
}


NVMainMemInterface::~NVMainMemInterface()
{
    std::cout << "NVMain dtor called" << std::endl;
}


void
NVMainMemInterface::init()
{
    std::cout << "NVMainMemInterface::init() starting" << std::endl;
    
    if( masterInstance == NULL )
    {
        masterInstance = this;
        std::cout << "Master instance set to this" << std::endl;

        m_nvmainPtr = new NVM::NVMain( );
        std::cout << "Created NVMain object" << std::endl;
        
        m_statsPtr = new NVM::Stats( );
        std::cout << "Created Stats object" << std::endl;
        
        m_nvmainSimInterface = new NVM::Gem5Interface( );
        std::cout << "Created Gem5Interface object" << std::endl;
        
        m_nvmainEventQueue = new NVM::EventQueue( );
        std::cout << "Created EventQueue object" << std::endl;
        
        m_nvmainGlobalEventQueue = new NVM::GlobalEventQueue( );
        std::cout << "Created GlobalEventQueue object" << std::endl;
        
        m_tagGenerator = new NVM::TagGenerator( 1000 );
        std::cout << "Created TagGenerator object" << std::endl;

        if (!m_nvmainConfig) {
            std::cerr << "Error: m_nvmainConfig is NULL" << std::endl;
            return;
        }
        std::cout << "m_nvmainConfig is not NULL" << std::endl;

        m_nvmainConfig->SetSimInterface( m_nvmainSimInterface );
        std::cout << "Set SimInterface" << std::endl;

        statPrinter.nvmainPtr = m_nvmainPtr;
        statReseter.nvmainPtr = m_nvmainPtr;
        std::cout << "Set stat pointers" << std::endl;

        if( m_nvmainConfig->KeyExists( "StatsFile" ) )
        {
            statPrinter.statStream.open( m_nvmainConfig->GetString( "StatsFile" ).c_str(),
                                         std::ofstream::out | std::ofstream::app );
            std::cout << "Opened stats file" << std::endl;
        }

        statPrinter.memory = this;
        statPrinter.forgdb = this;
        std::cout << "Set stat printer memory pointers" << std::endl;

        //registerExitCallback( &statPrinter );
        gem5::statistics::registerDumpCallback([this]() { statPrinter.process(); });
        gem5::statistics::registerResetCallback([this]() { statReseter.process(); });
        std::cout << "Registered dump and reset callbacks" << std::endl;

        SetEventQueue( m_nvmainEventQueue );
        SetStats( m_statsPtr );
        SetTagGenerator( m_tagGenerator );
        std::cout << "Set event queue, stats, and tag generator" << std::endl;

        // Set event queue on NVMain object before adding it to the global event queue
        if (m_nvmainPtr) {
            m_nvmainPtr->SetEventQueue( m_nvmainEventQueue );
            std::cout << "Set event queue on NVMain" << std::endl;
            
            m_nvmainPtr->SetStats( m_statsPtr );
            std::cout << "Set stats on NVMain" << std::endl;
            
            m_nvmainPtr->SetTagGenerator( m_tagGenerator );
            std::cout << "Set tag generator on NVMain" << std::endl;
        } else {
            std::cerr << "Error: m_nvmainPtr is NULL" << std::endl;
            return;
        }

        if (m_nvmainGlobalEventQueue && m_nvmainConfig) {
            if (m_nvmainConfig->KeyExists("CPUFreq")) {
                m_nvmainGlobalEventQueue->SetFrequency( m_nvmainConfig->GetValue( "CPUFreq" ) * 1000000.0 );
                std::cout << "Set frequency from config" << std::endl;
            } else {
                // Set default frequency if CPUFreq is not defined
                m_nvmainGlobalEventQueue->SetFrequency( 1000000000.0 ); // 1GHz
                std::cout << "Set default frequency" << std::endl;
            }
            SetGlobalEventQueue( m_nvmainGlobalEventQueue );
            std::cout << "Set global event queue" << std::endl;
        } else {
            std::cerr << "Error: m_nvmainGlobalEventQueue or m_nvmainConfig is NULL" << std::endl;
            return;
        }

        // TODO: Confirm global event queue frequency is the same as this SimObject's clock.

        /*  Add any specified hooks */
        std::vector<std::string>& hookList = m_nvmainConfig->GetHooks( );
        std::cout << "Number of hooks: " << hookList.size() << std::endl;

        for( size_t i = 0; i < hookList.size( ); i++ )
        {
            std::cout << "Creating hook " << hookList[i] << std::endl;

            NVMObject *hook = HookFactory::CreateHook( hookList[i] );

            if( hook != NULL )
            {
                AddHook( hook );
                std::cout << "Added hook " << hookList[i] << std::endl;
                
                hook->SetParent( this );
                std::cout << "Set parent for hook " << hookList[i] << std::endl;
                
                hook->Init( m_nvmainConfig );
                std::cout << "Initialized hook " << hookList[i] << std::endl;
            }
            else
            {
                std::cout << "Warning: Could not create a hook named `"
                    << hookList[i] << "'." << std::endl;
            }
        }

        /* Setup child and parent modules. */
        if (m_nvmainPtr) {
            AddChild( m_nvmainPtr );
            std::cout << "Added NVMain as child" << std::endl;
            
            m_nvmainPtr->SetParent( this );
            std::cout << "Set parent for NVMain" << std::endl;
            
            if (m_nvmainGlobalEventQueue && m_nvmainConfig) {
                std::cout << "About to add system to global event queue" << std::endl;
                m_nvmainGlobalEventQueue->AddSystem( m_nvmainPtr, m_nvmainConfig );
                std::cout << "Added system to global event queue" << std::endl;
            }
            
            m_nvmainPtr->SetConfig( m_nvmainConfig );
            std::cout << "Set config for NVMain" << std::endl;
        }

        masterInstance->allInstances.push_back(this);
        std::cout << "Added this to allInstances" << std::endl;
    }
    else
    {
        masterInstance->allInstances.push_back(this);
        masterInstance->otherInstance = this;
        std::cout << "Added this to existing master's allInstances" << std::endl;
    }
    
    std::cout << "NVMainMemInterface::init() completed" << std::endl;
}


void NVMainMemInterface::startup()
{
    DPRINTF(NVMain, "NVMainMemInterface: startup() called.\n");
    DPRINTF(NVMainMin, "NVMainMemInterface: startup() called.\n");

    /*
     *  Schedule the initial event. Needed for warmup and timing mode.
     *  If we are in atomic/fast-forward, wakeup will be disabled upon
     *  the first atomic request receieved in recvAtomic().
     */
    if (!masterInstance->clockEvent.scheduled())
        schedule(masterInstance->clockEvent, curTick() + clock);

    lastWakeup = curTick();
}


void NVMainMemInterface::wakeup()
{
    DPRINTF(NVMain, "NVMainMemInterface: wakeup() called.\n");
    DPRINTF(NVMainMin, "NVMainMemInterface: wakeup() called.\n");

    schedule(masterInstance->clockEvent, clockEdge());

    lastWakeup = curTick();
}


void NVMainMemInterface::NVMainStatPrinter::process()
{
    if (nvmainPtr == NULL || memory == NULL) {
        std::cerr << "Error: nvmainPtr or memory is NULL in stat printer" << std::endl;
        return;
    }

    if (memory->m_nvmainGlobalEventQueue == NULL) {
        std::cerr << "Error: m_nvmainGlobalEventQueue is NULL in stat printer" << std::endl;
        return;
    }

    assert(curTick() >= memory->lastWakeup);
    Tick stepCycles = (curTick() - memory->lastWakeup) / memory->clock;

    memory->m_nvmainGlobalEventQueue->Cycle( stepCycles );

    nvmainPtr->CalculateStats();
    std::ostream& refStream = (statStream.is_open()) ? statStream : std::cout;
    nvmainPtr->GetStats()->PrintAll( refStream );
}


void NVMainMemInterface::NVMainStatReseter::process()
{
    if (nvmainPtr == NULL) {
        std::cerr << "Error: nvmainPtr is NULL in stat reseter" << std::endl;
        return;
    }

    nvmainPtr->ResetStats();
    nvmainPtr->GetStats()->ResetAll( );
}


void
NVMainMemInterface::SetRequestData(NVMainRequest *request, PacketPtr pkt)
{
    uint8_t *hostAddr;

    request->data.SetSize( pkt->getSize() );
    request->oldData.SetSize( pkt->getSize() );

    if (pkt->isRead())
    {
        RequestPtr dataReq = std::make_shared<Request>(pkt->getAddr(), pkt->getSize(), 0, 0);
        Packet *dataPkt = new Packet(dataReq, MemCmd::ReadReq, pkt->getSize());
        dataPkt->allocate();
        doFunctionalAccess(dataPkt);

        hostAddr = new uint8_t[ pkt->getSize() ];
        memcpy( hostAddr, dataPkt->getPtr<uint8_t>(), pkt->getSize() );

        for(int i = 0; i < pkt->getSize(); i++ )
        {
            request->oldData.SetByte(i, *(hostAddr + i));
            request->data.SetByte(i, *(hostAddr + i));
        }

        delete dataPkt;
        delete [] hostAddr;
    }
    else
    {
        RequestPtr dataReq = std::make_shared<Request>(pkt->getAddr(), pkt->getSize(), 0, 0);
        Packet *dataPkt = new Packet(dataReq, MemCmd::ReadReq, pkt->getSize());
        dataPkt->allocate();
        doFunctionalAccess(dataPkt);

        uint8_t *hostAddrT = new uint8_t[ pkt->getSize() ];
        memcpy( hostAddrT, dataPkt->getPtr<uint8_t>(), pkt->getSize() );

        hostAddr = new uint8_t[ pkt->getSize() ];
        memcpy( hostAddr, pkt->getPtr<uint8_t>(), pkt->getSize() );

        for(int i = 0; i < pkt->getSize(); i++ )
        {
            request->oldData.SetByte(i, *(hostAddrT + i));
            request->data.SetByte(i, *(hostAddr + i));
        }

        delete dataPkt;
        delete [] hostAddrT;
        delete [] hostAddr;
    }
}


Tick NVMainMemInterface::doAtomicAccess(PacketPtr pkt)
{
    access(pkt);
    return static_cast<Tick>(m_avgAtomicLatency);
}


void NVMainMemInterface::doFunctionalAccess(PacketPtr pkt)
{
    functionalAccess(pkt);
}


DrainState NVMainMemInterface::drain()
{
    if( !masterInstance->m_request_map.empty() )
    {
        return DrainState::Draining;
    }
    else
    {
        return DrainState::Drained;
    }
}


void NVMainMemInterface::recvRetry( )
{
    DPRINTF(NVMain, "NVMainMemInterface: recvRetry() called.\n");
    DPRINTF(NVMainMin, "NVMainMemInterface: recvRetry() called.\n");

    retryResp = false;
    SendResponses( );
}


bool NVMainMemInterface::RequestComplete(NVM::NVMainRequest *req)
{
    bool isRead = (req->type == READ || req->type == READ_PRECHARGE);
    bool isWrite = (req->type == WRITE || req->type == WRITE_PRECHARGE);
    bool isSkyrmion = (req->type == INSERT || req->type == DELETE || req->type == LIM || req->type == PARALLEL);

    /* Ignore bus read/write requests generated by the banks. */
    if( req->type == BUS_WRITE || req->type == BUS_READ )
    {
        delete req;
        return true;
    }

    NVMainMemoryRequest *memRequest;
    std::map<NVMainRequest *, NVMainMemoryRequest *>::iterator iter;

    // Find the mem request pointer in the map.
    assert(masterInstance->m_request_map.count(req) != 0);
    iter = masterInstance->m_request_map.find(req);
    memRequest = iter->second;

    if(!memRequest->atomic)
    {
        bool respond = false;

        NVMainMemInterface *ownerInstance = dynamic_cast<NVMainMemInterface *>( req->owner );
        assert( ownerInstance != NULL );

        if( memRequest->packet )
        {
            respond = memRequest->packet->needsResponse();
            ownerInstance->access(memRequest->packet);
        }

        for( auto retryIter = masterInstance->allInstances.begin(); 
             retryIter != masterInstance->allInstances.end(); retryIter++ )
        {
            if( (*retryIter)->retryRead && (isRead || isWrite) )
            {
                (*retryIter)->retryRead = false;
                // (*retryIter)->port.sendRetryReq();
            }
            if( (*retryIter)->retryWrite && (isRead || isWrite) )
            {
                (*retryIter)->retryWrite = false;
                // (*retryIter)->port.sendRetryReq();
            }
        }

        DPRINTF(NVMain, "Completed Mem request for 0x%x of type %s\n", req->address.GetPhysicalAddress( ), (isRead ? "READ" : "WRITE"));

        if(respond)
        {
            ownerInstance->responseQueue.push_back(memRequest->packet);
            ownerInstance->ScheduleResponse( );

            delete req;
            delete memRequest;
        }
        else
        {
            if( memRequest->packet )
                ownerInstance->pendingDelete.push_back(memRequest->packet);

            CheckDrainState( );

            delete req;
            delete memRequest;
        }
    }
    else
    {
        delete req;
        delete memRequest;
    }


    masterInstance->m_request_map.erase(iter);
    //assert(m_requests_outstanding > 0);
    m_requests_outstanding--;

    return true;
}


void NVMainMemInterface::SendResponses( )
{
    if( responseQueue.empty() || retryResp == true )
        return;


    // bool success = port.sendTimingResp( responseQueue.front() );

    // if( success )
    // {
    //     DPRINTF(NVMain, "NVMainMemInterface: Sending response.\n");

    //     responseQueue.pop_front( );

    //     if( !responseQueue.empty( ) )
    //         ScheduleResponse( );

    //     CheckDrainState( );
    // }
    // else
    // {
    //     DPRINTF(NVMain, "NVMainMemInterface: Retrying response.\n");
    //     DPRINTF(NVMainMin, "NVMainMemInterface: Retrying response.\n");

    //     retryResp = true;
    // }
}


void NVMainMemInterface::CheckDrainState( )
{
    if( drainManager != NULL && masterInstance->m_request_map.empty() )
    {
        DPRINTF(NVMain, "NVMainMemInterface: Drain completed.\n");
        DPRINTF(NVMainMin, "NVMainMemInterface: Drain completed.\n");

        drainManager->signalDrainDone( );
        drainManager = NULL;
    }
}


void NVMainMemInterface::ScheduleResponse( )
{
    if( !respondEvent.scheduled( ) )
        schedule(respondEvent, curTick() + clock);
}


void NVMainMemInterface::ScheduleClockEvent( Tick nextWake )
{
    if( !masterInstance->clockEvent.scheduled() )
        schedule(masterInstance->clockEvent, nextWake);
    else
        reschedule(masterInstance->clockEvent, nextWake);
}


void NVMainMemInterface::serialize(CheckpointOut &cp) const
{
    if (masterInstance != this)
        return;

    std::string nvmain_chkpt_dir = "";

    if( m_nvmainConfig->KeyExists( "CheckpointDirectory" ) )
        nvmain_chkpt_dir = m_nvmainConfig->GetString( "CheckpointDirectory" );

    if( nvmain_chkpt_dir != "" )
    {
        std::cout << "NVMainMemInterface: Writing to checkpoint directory " << nvmain_chkpt_dir << std::endl;

        m_nvmainPtr->CreateCheckpoint( nvmain_chkpt_dir );
    }
}


void NVMainMemInterface::unserialize(CheckpointIn &cp)
{
    if (masterInstance != this)
        return;

    std::string nvmain_chkpt_dir = "";

    if( m_nvmainConfig->KeyExists( "CheckpointDirectory" ) )
        nvmain_chkpt_dir = m_nvmainConfig->GetString( "CheckpointDirectory" );

    if( nvmain_chkpt_dir != "" )
    {
        std::cout << "NVMainMemInterface: Reading from checkpoint directory " << nvmain_chkpt_dir << std::endl;

        m_nvmainPtr->RestoreCheckpoint( nvmain_chkpt_dir );
    }
}


void NVMainMemInterface::tick( )
{
    // Cycle memory controller
    if (masterInstance == this)
    {
        /* Keep NVMain in sync with gem5. */
        assert(curTick() >= lastWakeup);
        ncycle_t stepCycles = (curTick() - lastWakeup) / clock;

        DPRINTF(NVMain, "NVMainMemInterface: Stepping %d cycles\n", stepCycles);
        m_nvmainGlobalEventQueue->Cycle( stepCycles );

        lastWakeup = curTick();

        ncycle_t nextEvent;

        nextEvent = m_nvmainGlobalEventQueue->GetNextEvent(NULL);
        if( nextEvent != std::numeric_limits<ncycle_t>::max() )
        {
            ncycle_t currentCycle = m_nvmainGlobalEventQueue->GetCurrentCycle();

            assert(nextEvent >= currentCycle);
            stepCycles = nextEvent - currentCycle;

            Tick nextWake = curTick() + clock * static_cast<Tick>(stepCycles);

            DPRINTF(NVMain, "NVMainMemInterface: Next event: %d CurrentCycle: %d\n", nextEvent, currentCycle);
            DPRINTF(NVMain, "NVMainMemInterface: Schedule wake for %d\n", nextWake);

            nextEventCycle = nextEvent;
            ScheduleClockEvent( nextWake );
        }
    }
}


// MemInterface virtual functions implementation
void NVMainMemInterface::setupRank(const uint8_t rank, const bool is_read)
{
    // Implementation needed
}


bool NVMainMemInterface::allRanksDrained() const
{
    return masterInstance->m_request_map.empty();
}


std::pair<MemPacketQueue::iterator, Tick>
NVMainMemInterface::chooseNextFRFCFS(MemPacketQueue& queue, Tick min_col_at) const
{
    // Implementation needed
    return std::make_pair(queue.end(), min_col_at);
}


Tick NVMainMemInterface::accessLatency() const
{
    return static_cast<Tick>(m_avgAtomicLatency);
}


Tick NVMainMemInterface::commandOffset() const
{
    return tBURST;
}


bool NVMainMemInterface::burstReady(MemPacket* pkt) const
{
    // Implementation needed
    return true;
}


void NVMainMemInterface::addRankToRankDelay(Tick cmd_at)
{
    // Implementation needed
}


bool NVMainMemInterface::isBusy(bool read_queue_empty, bool all_writes_nvm)
{
    // Implementation needed
    return false;
}


std::pair<Tick, Tick>
NVMainMemInterface::doBurstAccess(MemPacket* mem_pkt, Tick next_burst_at,
                  const std::vector<MemPacketQueue>& queue)
{
    // Implementation needed
    return std::make_pair(next_burst_at, next_burst_at + tBURST);
}


NVMainMemInterface *
NVMainMemInterfaceParams::create() const
{
    return new NVMainMemInterface(this);
}
