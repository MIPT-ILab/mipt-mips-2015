/*
 * fetch.h - simulator of fetch unit
 * Copyright 2015-2018 MIPT-MIPS
 */

#ifndef FETCH_H
#define FETCH_H

#include "bpu/bpu.h"

#include <func_sim/instr_memory.h>
#include <infra/cache/cache_tag_array.h>
#include <modules/core/perf_instr.h>
#include <modules/ports_instance.h>
 
template <typename FuncInstr>
class Fetch : public Module
{
    using Instr = PerfInstr<FuncInstr>;

public:
    explicit Fetch( Module* parent);
    void clock( Cycle cycle);
    void set_memory( std::unique_ptr<InstrMemoryIface<FuncInstr>> mem)
    {
        memory = std::move( mem);
    }

private:
    std::unique_ptr<InstrMemoryIface<FuncInstr>> memory = nullptr;
    std::unique_ptr<BaseBP> bp = nullptr;
    std::unique_ptr<CacheTagArray> tags = nullptr;

    /* counter for instruction_id implementation */
    int curr_id = 0;
    
    /* Input signals */
    ReadPort<bool>* rp_stall = nullptr;
    ReadPort<bool>* rp_hit_or_miss = nullptr;

    /* Input signals - BP */
    ReadPort<BPInterface>* rp_bp_update = nullptr;
    ReadPort<BPInterface>* rp_bp_update_from_decode = nullptr;
    
    /* Input signals - PC values */
    ReadPort<Target>* rp_flush_target = nullptr;
    ReadPort<Target>* rp_external_target = nullptr;
    ReadPort<Target>* rp_hold_pc = nullptr;
    ReadPort<Target>* rp_target = nullptr;
    ReadPort<Target>* rp_long_latency_pc_holder = nullptr;

    /* Outputs */
    WritePort<Instr>* wp_datapath = nullptr;
    WritePort<Target>* wp_hold_pc = nullptr;
    WritePort<Target>* wp_target = nullptr;
    WritePort<Target>* wp_long_latency_pc_holder = nullptr;
    WritePort<bool>* wp_hit_or_miss = nullptr;

    /* port needed for handling misprediction at decode stage */
    ReadPort<Target>* rp_flush_target_from_decode = nullptr;

    Target get_target( Cycle cycle);
    Target get_cached_target( Cycle cycle);
    void clock_bp( Cycle cycle);
    void clock_instr_cache( Cycle cycle);
    void save_flush( Cycle cycle);

    uint32 _fetchahead_size; // value of fetchahead distance size
    std::string _prefetch_method; // value of prefetch method

    bool is_wrong_path = false;
    void prefetch_next_line(Addr requested_addr);
};

struct PrefetchMethodException final : Exception
{
    explicit PrefetchMethodException( const std::string& msg)
            : Exception( "Invalid prefetch method name", msg)
    { }
};

#endif
