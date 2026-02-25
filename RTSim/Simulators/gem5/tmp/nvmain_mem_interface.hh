/*
 * Copyright (c) 2012-2013 Pennsylvania State University
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

#ifndef __MEM_NVMAIN_MEM_INTERFACE_HH__
#define __MEM_NVMAIN_MEM_INTERFACE_HH__


#include <fstream>
#include <ostream>
#include <deque>
#include <vector>
#include <map>

#include "NVM/nvmain.h"
#include "base/callback.hh"
#include "include/NVMTypes.h"
#include "include/NVMainRequest.h"
#include "mem/mem_interface.hh"
#include "mem/packet.hh"
#include "params/NVMainMemInterface.hh"
#include "sim/eventq.hh"
#include "sim/serialize.hh"
#include "src/Config.h"
#include "src/EventQueue.h"
#include "src/NVMObject.h"
#include "src/SimInterface.h"
#include "src/TagGenerator.h"

using namespace gem5;
using namespace gem5::memory;

class NVMainMemInterface : public MemInterface, public NVM::NVMObject
{
  private:

    void tick();
    void SendResponses( );
    EventWrapper<NVMainMemInterface, &NVMainMemInterface::tick> clockEvent;
    EventWrapper<NVMainMemInterface, &NVMainMemInterface::SendResponses> respondEvent;

    void CheckDrainState( );
    void ScheduleResponse( );
    void ScheduleClockEvent( Tick );
    void SetRequestData(NVM::NVMainRequest *request, PacketPtr pkt);

    class NVMainStatPrinter
    {
        friend class NVMainMemInterface;

      public:
        NVMainMemInterface *memory;
        NVMainMemInterface *forgdb;

        std::function<void()> process();

        NVM::NVMain *nvmainPtr;
        std::ofstream statStream;
    };

    class NVMainStatReseter 
    {
      public:
        std::function<void()> process();

        NVM::NVMain *nvmainPtr;
    };

    struct NVMainMemoryRequest
    {
        PacketPtr packet;
        NVM::NVMainRequest *request;
        Tick issueTick;
        bool atomic;
    };

    DrainManager *drainManager;

    NVM::NVMain *m_nvmainPtr;
    NVM::Stats *m_statsPtr;
    NVM::EventQueue *m_nvmainEventQueue;
    NVM::GlobalEventQueue *m_nvmainGlobalEventQueue;
    NVM::Config *m_nvmainConfig;
    NVM::SimInterface *m_nvmainSimInterface;
    NVM::TagGenerator *m_tagGenerator;
    std::string m_nvmainConfigPath;

    bool m_nacked_requests;
    float m_avgAtomicLatency;
    uint64_t m_numAtomicAccesses;
    NVM::ncycle_t nextEventCycle;

    Tick clock;
    Tick lat;
    Tick lat_var;
    bool nvmain_atomic;

    uint64_t BusWidth;
    uint64_t tBURST;
    uint64_t RATE;

    bool NVMainWarmUp;

    NVMainStatPrinter statPrinter;
    NVMainStatReseter statReseter;
    Tick lastWakeup;

    uint64_t m_requests_outstanding;

  public:

    typedef NVMainMemInterfaceParams Params;
    NVMainMemInterface(const Params *p);
    virtual ~NVMainMemInterface();

    void init() override;
    void startup() override;
    void wakeup();

    const Params *
    params() const
    {
        return &static_cast<const Params &>(_params);
    }


    bool RequestComplete( NVM::NVMainRequest *req ) override;

    void Cycle(NVM::ncycle_t) override { }

    DrainState drain() override;

    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    static NVMainMemInterface *masterInstance;
    NVMainMemInterface *otherInstance;
    std::vector<NVMainMemInterface *> allInstances;
    bool retryRead, retryWrite, retryResp;
    std::deque<PacketPtr> responseQueue;
    std::vector<PacketPtr> pendingDelete;
    std::map<NVM::NVMainRequest *, NVMainMemoryRequest *> m_request_map;

  protected:

    Tick doAtomicAccess(PacketPtr pkt);
    void doFunctionalAccess(PacketPtr pkt);
    void recvRetry();

    // MemInterface virtual functions
    void setupRank(const uint8_t rank, const bool is_read) override;
    bool allRanksDrained() const override;
    std::pair<MemPacketQueue::iterator, Tick>
    chooseNextFRFCFS(MemPacketQueue& queue, Tick min_col_at) const override;
    Tick accessLatency() const override;
    Tick commandOffset() const override;
    bool burstReady(MemPacket* pkt) const override;
    void addRankToRankDelay(Tick cmd_at) override;
    bool isBusy(bool read_queue_empty, bool all_writes_nvm) override;
    std::pair<Tick, Tick>
    doBurstAccess(MemPacket* mem_pkt, Tick next_burst_at,
                  const std::vector<MemPacketQueue>& queue) override;

};

#endif
