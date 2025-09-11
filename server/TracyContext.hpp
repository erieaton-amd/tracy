#ifndef __TRACYCONTEXT_HPP__
#define __TRACYCONTEXT_HPP__

#include "TracyEvent.hpp"
#include "TracyShortPtr.hpp"
#include "tracy_robin_hood.h"

namespace tracy
{

constexpr const char* ZoneContextNames[] = {
    "Invalid",
    "OpenGL",
    "Vulkan",
    "OpenCL",
    "Direct3D 12",
    "Direct3D 11",
    "Metal",
    "Custom",
    "CUDA",
    "Rocprof",
    "CPU"
};

struct ZoneContext;
class Worker;

struct ThreadData
{
    uint64_t id;
    uint64_t count;
    Vector<short_ptr<ZoneEvent>> timeline;
    Vector<short_ptr<ZoneEvent>> stack;
    Vector<short_ptr<MessageData>> messages;
    uint32_t nextZoneId;
    Vector<uint32_t> zoneIdStack;
    uint8_t isFiber;
    ThreadData* fiber;
    uint8_t* stackCount;
    int32_t groupHint;
    ZoneContext* ctx;
#ifndef TRACY_NO_STATISTICS
  Vector<int64_t> childTimeStack;
#endif

    tracy_force_inline void IncStackCount( int16_t srcloc ) { stackCount[uint16_t( srcloc )]++; }
    tracy_force_inline bool DecStackCount( int16_t srcloc ) { return --stackCount[uint16_t( srcloc )] != 0; }
};

struct ZoneContext
{
    struct ZoneThreadData
    {
        tracy_force_inline ZoneEvent* Zone() const { return (ZoneEvent*)( _zone_thread >> 16 ); }
        tracy_force_inline void SetZone( ZoneEvent* zone )
        {
            auto z64 = (uint64_t)zone;
            assert( ( z64 & 0xFFFF000000000000 ) == 0 );
            memcpy( ( (char*)&_zone_thread ) + 2, &z64, 4 );
            memcpy( ( (char*)&_zone_thread ) + 6, ( (char*)&z64 ) + 4, 2 );
        }
        tracy_force_inline uint16_t Thread() const { return uint16_t( _zone_thread & 0xFFFF ); }
        tracy_force_inline void SetThread( uint16_t thread ) { memcpy( &_zone_thread, &thread, 2 ); }

        uint64_t _zone_thread;
    };
    enum
    {
        ZoneThreadDataSize = sizeof( ZoneThreadData )
    };

private:
    struct SourceLocationZones
    {
        struct ZtdSort
        {
            bool operator()( const ZoneThreadData& lhs, const ZoneThreadData& rhs ) const { return lhs.Zone()->Start() < rhs.Zone()->Start(); }
        };

        SortedVector<ZoneThreadData, ZtdSort> zones;
        int64_t min = std::numeric_limits<int64_t>::max();
        int64_t max = std::numeric_limits<int64_t>::min();
        int64_t total = 0;
        double sumSq = 0;
        int64_t selfMin = std::numeric_limits<int64_t>::max();
        int64_t selfMax = std::numeric_limits<int64_t>::min();
        int64_t selfTotal = 0;
        size_t nonReentrantCount = 0;
        int64_t nonReentrantMin = std::numeric_limits<int64_t>::max();
        int64_t nonReentrantMax = std::numeric_limits<int64_t>::min();
        int64_t nonReentrantTotal = 0;
        unordered_flat_map<uint16_t, uint64_t> threadCnt;
    };

#ifndef TRACY_NO_STATISTICS
    unordered_flat_map<int16_t, SourceLocationZones> sourceLocationZones;
    bool sourceLocationZonesReady = false;
#else
    unordered_flat_map<int16_t, uint64_t> sourceLocationZonesCnt;
#endif

#ifndef TRACY_NO_STATISTICS
    std::pair<uint16_t, SourceLocationZones*> srclocZonesLast = std::make_pair( 0, nullptr );
#else
    std::pair<uint16_t, uint64_t*> srclocCntLast = std::make_pair( 0, nullptr );
#endif

public:
    StringIdx name;
    std::string longName;
    ZoneContextType type;
    unordered_flat_map<uint64_t, ThreadData*> threadData;
    Vector<ThreadData*> threads;
    uint64_t count;
    unordered_flat_map<int64_t, StringIdx> noteNames;
    unordered_flat_map<uint16_t, unordered_flat_map<int64_t, double>> notes;
    Vector<ZoneExtra> zoneExtra;

    uint64_t threadCtx = 0;
    ThreadData* threadCtxData = nullptr;
    int64_t refTimeThread = 0;

    ZoneContext() { zoneExtra.push_back( ZoneExtra {} ); }

    const ThreadData* GetThreadData( uint64_t tid ) const;

    SourceLocationZones& GetZonesForSourceLocation( int16_t srcloc );
    const SourceLocationZones& GetZonesForSourceLocation( int16_t srcloc ) const;
    const unordered_flat_map<int16_t, SourceLocationZones>& GetSourceLocationZones() const { return sourceLocationZones; }
    bool AreSourceLocationZonesReady() const { return sourceLocationZonesReady; }
    void SetSourceLocationZonesReady() { sourceLocationZonesReady = true; }

    tracy_force_inline const bool HasZoneExtra( const ZoneEvent& ev ) const { return ev.extra != 0; }
    uint64_t GetZoneExtraCount() const { return zoneExtra.size() - 1; }
    tracy_force_inline const ZoneExtra& GetZoneExtra( const ZoneEvent& ev ) const { return zoneExtra[ev.extra]; }
    tracy_force_inline ZoneExtra& GetZoneExtraMutable( const ZoneEvent& ev ) { return zoneExtra[ev.extra]; }
    tracy_force_inline ZoneExtra& AllocZoneExtra( ZoneEvent& ev )
    {
        assert( ev.extra == 0 );
        ev.extra = uint32_t( zoneExtra.size() );
        auto& extra = zoneExtra.push_next();
        memset( (char*)&extra, 0, sizeof( extra ) );
        return extra;
    }

    tracy_force_inline ZoneExtra& RequestZoneExtra( ZoneEvent& ev )
    {
        if( !HasZoneExtra( ev ) )
        {
            return AllocZoneExtra( ev );
        }
        else
        {
            return GetZoneExtraMutable( ev );
        }
    }

#ifndef TRACY_NO_STATISTICS
    SourceLocationZones* GetSourceLocationZones( uint16_t srcloc )
    {
        if( srclocZonesLast.first == srcloc ) return srclocZonesLast.second;
        return GetSourceLocationZonesReal( srcloc );
    }
    SourceLocationZones* GetSourceLocationZonesReal( uint16_t srcloc );
    void InitSourceLocationZones( uint16_t srcloc );
#else
    uint64_t* GetSourceLocationZonesCnt( uint16_t srcloc )
    {
        if( srclocCntLast.first == srcloc ) return srclocCntLast.second;
        return GetSourceLocationZonesCntReal( srcloc );
    }
    uint64_t* GetSourceLocationZonesCntReal( uint16_t srcloc );
    void InitSourceLocationZonesCnt( uint16_t srcloc );
#endif

    friend class Worker;
};

struct CPUZoneContext : public ZoneContext
{
    CPUZoneContext() { type = ZoneContextType::CPU; }
};

struct CPUThreadData : public ThreadData
{
#ifndef TRACY_NO_STATISTICS
    Vector<GhostZone> ghostZones;
    uint64_t ghostIdx;
    SortedVector<SampleData, SampleDataSort> postponedSamples;
#endif
    Vector<SampleData> samples;
    SampleData pendingSample;
    Vector<SampleData> ctxSwitchSamples;
    uint64_t kernelSampleCnt;
};

struct GpuCtxData : public ZoneContext
{
    int64_t timeDiff;
    uint64_t thread;
    float period;
    bool hasPeriod;
    bool hasCalibration;
    int64_t calibratedGpuTime;
    int64_t calibratedCpuTime;
    double calibrationMod;
    int64_t lastGpuTime;
    uint64_t overflow;
    uint32_t overflowMul;
    short_ptr<ZoneEvent> query[64 * 1024];
};

enum
{
    GpuCtxDataSize = sizeof( GpuCtxData )
};

// Used to carry the context of a zone around with the zone. This information is not stored in the
// zone for space reasons, and is expensive to recover. This struct needs to remain small because it
// is passed by value.
struct ZoneEventC
{
    const ZoneEvent *event = nullptr;
    const ZoneContext *ctx = nullptr;

    // assume no ZoneEvents appear in two contexts, and save a comparison
    tracy_force_inline bool operator==(const ZoneEventC& other) const { return other.event == event; }
    tracy_force_inline operator bool() const { return event == nullptr; }
    tracy_force_inline const ZoneExtra& Extra() const { return ctx->GetZoneExtra( *event ); }
    tracy_force_inline const ZoneEvent* operator->() const { return event; }
};

struct ZoneEventCT : public ZoneEventC
{
    const ThreadData *thread = nullptr;
};

} // namespace tracy

#endif /* TRACYCONTEXT_H */
