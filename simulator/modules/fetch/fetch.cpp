/*
 * fetch.cpp - simulator of fetch unit
 * Copyright 2015-2018 MIPT-MIPS
 */

#include <infra/config/config.h>

#include "fetch.h"

namespace config {
    /* Cache parameters */
    static const Value<std::string> instruction_cache_type = { "icache-type", "LRU", "Type of instruction level 1 cache (in bytes)"};
    static const Value<uint32> instruction_cache_size = { "icache-size", 2048, "Size of instruction level 1 cache (in bytes)"};
    static const Value<uint32> instruction_cache_ways = { "icache-ways", 4, "Amount of ways in instruction level 1 cache"};
    static const Value<uint32> instruction_cache_line_size = { "icache-line-size", 64, "Line size of instruction level 1 cache (in bytes)"};
    /* Prefetch parameters */
    static const Value<uint32> fetchahead_distance = { "fetchahead-size", 32, "Fetchahead distance size"};
    static const Value<std::string> prefetch_method = { "prefetch-method", "wrong-path", "Type of a Instruction prefetching method"};
} // namespace config

template <typename FuncInstr>
Fetch<FuncInstr>::Fetch( Module* parent) : Module( parent, "fetch")
{
    wp_datapath = make_write_port<Instr>("FETCH_2_DECODE", Port::BW);
    rp_stall = make_read_port<bool>("DECODE_2_FETCH_STALL", Port::LATENCY);

    rp_flush_target = make_read_port<Target>("BRANCH_2_FETCH_TARGET", Port::LATENCY);

    wp_target = make_write_port<Target>("TARGET", Port::BW);
    rp_target = make_read_port<Target>("TARGET", Port::LATENCY);

    wp_hold_pc = make_write_port<Target>("HOLD_PC", Port::BW);
    rp_hold_pc = make_read_port<Target>("HOLD_PC", Port::LATENCY);

    rp_external_target = make_read_port<Target>("WRITEBACK_2_FETCH_TARGET", Port::LATENCY);

    rp_bp_update = make_read_port<BPInterface>("BRANCH_2_FETCH", Port::LATENCY);

    wp_long_latency_pc_holder = make_write_port<Target>("LONG_LATENCY_PC_HOLDER", Port::BW);
    rp_long_latency_pc_holder = make_read_port<Target>("LONG_LATENCY_PC_HOLDER", Port::LONG_LATENCY);

    wp_hit_or_miss = make_write_port<bool>("HIT_OR_MISS", Port::BW);
    rp_hit_or_miss = make_read_port<bool>("HIT_OR_MISS", Port::LATENCY);

    /* port needed for handling misprediction at decode stage */
    rp_bp_update_from_decode = make_read_port<BPInterface>("DECODE_2_FETCH", Port::LATENCY);
    rp_flush_target_from_decode = make_read_port<Target>("DECODE_2_FETCH_TARGET", Port::LATENCY);

    bp = BaseBP::create_configured_bp();
    tags = CacheTagArray::create(
        config::instruction_cache_type,
        config::instruction_cache_size,
        config::instruction_cache_ways,
        config::instruction_cache_line_size,
        32
    );

    _fetchahead_size = config::fetchahead_distance;
    _prefetch_method = config::prefetch_method;

    if ( _prefetch_method != "next-line" && _prefetch_method != "wrong-path" && _prefetch_method != "no-prefetch")
        throw PrefetchMethodException("\"" + _prefetch_method +
            "\" prefetch method is not defined, supported methods are:\nnext-line\nwrong-path\nno-prefetch\n");
}

template <typename FuncInstr>
Target Fetch<FuncInstr>::get_target( Cycle cycle)
{
    /* receive flush and stall signals */
    const bool is_stall = rp_stall->is_ready( cycle) && rp_stall->read( cycle);

    /* Receive all possible PC */
    const Target external_target = rp_external_target->is_ready( cycle) ? rp_external_target->read( cycle) : Target();
    const Target hold_target     = rp_hold_pc->is_ready( cycle) ? rp_hold_pc->read( cycle) : Target();
    const Target flushed_target  = rp_flush_target->is_ready( cycle) ? rp_flush_target->read( cycle) : Target();
    const Target flushed_target_from_decode = rp_flush_target_from_decode->is_ready( cycle) ?
                                                                rp_flush_target_from_decode->read( cycle) : Target();
    const Target branch_target   = rp_target->is_ready( cycle) ? rp_target->read( cycle) : Target();

    if ( _prefetch_method == "wrong-path")
        is_wrong_path = false;

    /* Multiplexing */
    if ( external_target.valid)
        return external_target;

    if ( flushed_target.valid)
        return flushed_target;

    if ( flushed_target_from_decode.valid)
    {
        if ( _prefetch_method == "wrong-path") // prefetch wrong path if prefetch enabled and defined
        {
            is_wrong_path = true;
            if ( !tags->lookup( flushed_target_from_decode.address))
                tags->write( flushed_target_from_decode.address);
        }
        return flushed_target_from_decode;
    }

    if ( !is_stall && branch_target.valid)
        return branch_target;

    if ( hold_target.valid)
        return hold_target;

    return Target();
}

template <typename FuncInstr>
void Fetch<FuncInstr>::clock_bp( Cycle cycle)
{
    /* Process BP updates */
    if ( rp_bp_update->is_ready( cycle))
        bp->update( rp_bp_update->read( cycle));
    if ( rp_bp_update_from_decode->is_ready( cycle))
        bp->update( rp_bp_update_from_decode->read(cycle));
}

template <typename FuncInstr>
void Fetch<FuncInstr>::clock_instr_cache( Cycle cycle)
{
    if ( rp_long_latency_pc_holder->is_ready( cycle))
    {
        /* get PC from long-latency port if it's possible */
        auto target = rp_long_latency_pc_holder->read( cycle);

        /* write new PC to tags array */
        tags->write( target.address);

        /* save PC to the next stage */
        wp_hold_pc->write( target, cycle);
        return;
    }
    wp_hit_or_miss->write( false, cycle);
}

template <typename FuncInstr>
void Fetch<FuncInstr>::save_flush( Cycle cycle)
{
    /* save PC in the case of flush signal */
    if( rp_flush_target->is_ready( cycle))
        wp_target->write( rp_flush_target->read( cycle), cycle);
    else if( rp_flush_target_from_decode->is_ready( cycle))
        wp_target->write( rp_flush_target_from_decode->read( cycle), cycle);
    else if( rp_target->is_ready( cycle))
        wp_target->write( rp_target->read( cycle), cycle);
    else if( rp_external_target->is_ready( cycle))
        wp_target->write( rp_external_target->read( cycle), cycle);
}

template <typename FuncInstr>
Target Fetch<FuncInstr>::get_cached_target( Cycle cycle)
{
    /* simulate request to the memory in the case of cache miss */
    if ( rp_hit_or_miss->is_ready( cycle))
    {
        save_flush( cycle);
        clock_instr_cache( cycle);
        return Target();
    }

    /* getting PC */
    auto target = get_target( cycle);

    /* push bubble */
    if ( !target.valid)
        return Target();

    /* hit or miss */
    auto is_hit = tags->lookup( target.address);

    if ( is_hit)
        return target;

    /* send miss to the next cycle */
    wp_hit_or_miss->write( is_hit, cycle);

    /* send PC to cache*/
    wp_long_latency_pc_holder->write( target, cycle);
    return Target();
}


template <typename FuncInstr>
void Fetch<FuncInstr>::clock( Cycle cycle)
{
    clock_bp( cycle);

    /* getting PC */
    auto target = get_cached_target( cycle);

    /* push bubble */
    if ( !target.valid)
        return;

    /* hold PC for the stall case */
    wp_hold_pc->write( target, cycle);

    auto bp_info = bp->get_bp_info( target.address);
    Instr instr( memory->fetch_instr( target.address), bp_info);
    instr.set_sequence_id( target.sequence_id);

    /* instruction unique id initialization */
    instr.set_instruction_id(curr_id++);

    /* set next target according to prediction */
    wp_target->write( instr.get_predicted_target(), cycle);

    /* log */
    sout << "fetch   cycle " << std::dec << cycle << ": " << instr << " " << bp_info << std::endl;

    /* sending to decode */
    wp_datapath->write( std::move( instr), cycle);

    /* JSON dump of Fetch-stage of executing operation */
    if (jsonout_enabled)
        (jsonout()) << ",\n\t{ \"type\": \"Record\", \"id\": " << instr.get_instruction_id() << ", \"disassembly\": \"" << instr.get_disasm() << "\" }" <<
        ",\n\t{ \"type\": \"Event\", \"id\": " << instr.get_instruction_id() << ", \"cycle\": " << cycle << ", \"stage\": 0 }";

    if ( _prefetch_method != "no-prefetch" && !is_wrong_path)  // prefetch next line if enabled and wrong path isn't used
        prefetch_next_line( target.address);
}

template<typename FuncInstr>
void Fetch<FuncInstr>::prefetch_next_line( Addr requested_addr)
{
    uint32 line_size( config::instruction_cache_line_size); // line size
    size_t line_bits = std::countr_zero( line_size); // number of offset bits

    Addr addr_mask = bitmask<Addr>( line_bits); // bit mask to extract the offset value
    Addr offset = requested_addr & addr_mask; // offset

    Addr next_line_addr = requested_addr + line_size; // the address that will be on the next line
    uint32 fetchahead_distance = _fetchahead_size; // setting the fetchahead

   /* if the fetchahead is reached and there is no line in the cache, write the line to the cache */
   if ( offset >= (line_size - fetchahead_distance) && !tags->lookup( next_line_addr))
       tags->write( next_line_addr);
}

#include <mips/mips.h>
#include <risc_v/risc_v.h>

template class Fetch<BaseMIPSInstr<uint32>>;
template class Fetch<BaseMIPSInstr<uint64>>;
template class Fetch<RISCVInstr<uint32>>;
template class Fetch<RISCVInstr<uint64>>;
template class Fetch<RISCVInstr<uint128>>;
