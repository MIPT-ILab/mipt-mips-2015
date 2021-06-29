/**
 * decode.cpp - simulation of decode stage
 * Copyright 2015-2018 MIPT-MIPS
 */

#include "decode.h"

#include <modules/execute/execute.h>

template <typename FuncInstr>
Decode<FuncInstr>::Decode( Module* parent) : Module( parent, "decode")
{
    bypassing_unit = std::make_unique<BypassingUnit>( config::long_alu_latency);

    rp_datapath = make_read_port<Instr>("FETCH_2_DECODE", Port::LATENCY);
    rp_stall_datapath = make_read_port<Instr>("DECODE_2_DECODE", Port::LATENCY);
    rp_flush = make_read_port<bool>("BRANCH_2_ALL_FLUSH", Port::LATENCY);
    rp_bypassing_unit_notify = make_read_port<Instr>("DECODE_2_BYPASSING_UNIT_NOTIFY", Port::LATENCY);
    rp_bypassing_unit_flush_notify = make_read_port<bool>("BRANCH_2_BYPASSING_UNIT_FLUSH_NOTIFY", Port::LATENCY);
    rp_flush_fetch = make_read_port<bool>("DECODE_2_FETCH_FLUSH", Port::LATENCY);
    rp_trap = make_read_port<bool>("WRITEBACK_2_ALL_FLUSH", Port::LATENCY);

    wp_datapath = make_write_port<Instr>("DECODE_2_EXECUTE", Port::BW);
    wp_stall_datapath = make_write_port<Instr>("DECODE_2_DECODE", Port::BW);
    wp_stall = make_write_port<bool>("DECODE_2_FETCH_STALL", Port::BW);
    wps_command[0] = make_write_port<BypassCommand<Register>>("DECODE_2_EXECUTE_SRC1_COMMAND", Port::BW);
    wps_command[1] = make_write_port<BypassCommand<Register>>("DECODE_2_EXECUTE_SRC2_COMMAND", Port::BW);
    wp_bypassing_unit_notify = make_write_port<Instr>("DECODE_2_BYPASSING_UNIT_NOTIFY", Port::BW);
    wp_flush_fetch = make_write_port<bool>("DECODE_2_FETCH_FLUSH", Port::BW);
    wp_flush_target = make_write_port<Target>("DECODE_2_FETCH_TARGET", Port::BW);
    wp_bp_update = make_write_port<BPInterface>("DECODE_2_FETCH", Port::BW);
}

template <typename FuncInstr>
auto Decode<FuncInstr>::read_instr( Cycle cycle) const
{
    if ( rp_stall_datapath->is_ready( cycle))
        return std::pair{ rp_stall_datapath->read( cycle), true};

    return std::pair{ rp_datapath->read( cycle), false};
}

template <typename FuncInstr>
bool Decode<FuncInstr>::is_misprediction( const Instr& instr, const BPInterface& bp_data)
{
    if ( ( instr.is_direct_jump() || instr.is_indirect_jump()) && !bp_data.is_taken)
        return true;

    // 'likely' branches, which are not in BTB, are purposely considered as mispredictions
    if ( instr.is_likely_branch() && !bp_data.is_hit)
        return true;

    return ( ( instr.is_direct_jump() || instr.is_branch())
        && bp_data.target != instr.get_decoded_target()
        && bp_data.is_taken);
}

template<typename FuncInstr>
bool Decode<FuncInstr>::is_flush( Cycle cycle) const
{
    return ( rp_flush->is_ready( cycle) && rp_flush->read( cycle))
        || ( rp_flush_fetch->is_ready( cycle) && rp_flush_fetch->read( cycle));
}

template <typename FuncInstr>
void Decode<FuncInstr>::clock( Cycle cycle)
{
    sout << "decode  cycle " << std::dec << cycle << ": ";

    const bool has_trap = rp_trap->is_ready( cycle) && rp_trap->read( cycle);
    const bool has_flush = is_flush( cycle);

    bypassing_unit->update();

    /* trace new instruction if needed */
    if ( rp_bypassing_unit_notify->is_ready( cycle))
    {
        auto instr = rp_bypassing_unit_notify->read( cycle);
        bypassing_unit->trace_new_instr( instr);
    }

    /* update bypassing unit because of misprediction */
    if ( rp_bypassing_unit_flush_notify->is_ready( cycle) || has_trap)
        bypassing_unit->handle_flush();

    if ( has_flush || has_trap)
    {
        sout << "flush\n";
        return;
    }

    /* check if there is something to process */
    if ( !rp_datapath->is_ready( cycle) && !rp_stall_datapath->is_ready( cycle))
    {
        sout << "bubble\n";
        return;
    }

    auto[instr, from_stall] = read_instr( cycle);

    if ( instr.is_jump())
        num_jumps++;

    /* handle misprediction */
    if ( is_misprediction( instr, instr.get_bp_data()))
    {
        num_mispredictions++;

        /* acquiring real information for BPU */
        wp_bp_update->write( instr.get_bp_upd(), cycle);

        // flushing fetch stage, instr fetch will appear at decode stage next clock,
        // so we send flush signal to decode
        if ( !bypassing_unit->is_stall( instr))
            wp_flush_fetch->write( true, cycle);

        /* sending valid PC to fetch stage */
        if ( !from_stall)
        {
            wp_flush_target->write( instr.get_actual_decoded_target(), cycle);
            sout << "\nmisprediction on ";
        }
    }

    if ( bypassing_unit->is_stall( instr))
    {
        // data hazard, stalling pipeline
        wp_stall->write( true, cycle);
        wp_stall_datapath->write( instr, cycle);
        sout << instr << " (data hazard)\n";
        return;
    }

    for ( size_t src_index = 0; src_index < SRC_REGISTERS_NUM; src_index++)
    {
        if ( bypassing_unit->is_in_RF( instr, src_index))
        {
            rf->read_source( &instr, src_index);
        }
        else if ( bypassing_unit->is_bypassible( instr, src_index))
        {
            const auto bypass_command = bypassing_unit->get_bypass_command( instr, src_index);
            wps_command.at( src_index)->write( bypass_command, cycle);
        }
    }

    /* notify bypassing unit about new instruction */
    wp_bypassing_unit_notify->write( instr, cycle);

    /* log */
    sout << instr << std::endl;

    wp_datapath->write( std::move( instr), cycle);

    /* JSON dump of Decode-stage of executing operation */
    if (jsonout_enabled)
        (jsonout()) << ",\n\t{ \"type\": \"Event\", \"id\": " << instr.get_instruction_id() << ", \"cycle\": " << cycle << ", \"stage\": 1 }";
}


#include <mips/mips.h>
#include <risc_v/risc_v.h>

template class Decode<BaseMIPSInstr<uint32>>;
template class Decode<BaseMIPSInstr<uint64>>;
template class Decode<RISCVInstr<uint32>>;
template class Decode<RISCVInstr<uint64>>;
template class Decode<RISCVInstr<uint128>>;

