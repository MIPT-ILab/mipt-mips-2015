/**
 * execute.cpp - Simulation of execute units
 * Copyright 2015-2018 MIPT-MIPS
 */

#include <infra/config/config.h>

#include "execute.h"

namespace config {
    const PredicatedValue<uint64> long_alu_latency = { "long-alu-latency", 3, "Latency of long arithmetic logic unit",
                                                [](uint64 val) { return val >= 2 && val < 64; } };
} // namespace config

template <typename FuncInstr>
Execute<FuncInstr>::Execute( Module* parent) : Module( parent, "execute")
    , last_execution_stage_latency( Latency( config::long_alu_latency - 1))
{
    wp_mem_datapath = make_write_port<Instr>("EXECUTE_2_MEMORY" , Port::BW );
    wp_branch_datapath = make_write_port<Instr>("EXECUTE_2_BRANCH" , Port::BW );
    wp_writeback_datapath = make_write_port<Instr>("EXECUTE_2_WRITEBACK", Port::BW);
    rp_datapath = make_read_port<Instr>("DECODE_2_EXECUTE", Port::LATENCY);
    rp_trap = make_read_port<bool>("WRITEBACK_2_ALL_FLUSH", Port::LATENCY);

    wp_long_latency_execution_unit = make_write_port<Instr>("EXECUTE_2_EXECUTE_LONG_LATENCY", Port::BW);
    rp_long_latency_execution_unit = make_read_port<Instr>("EXECUTE_2_EXECUTE_LONG_LATENCY", last_execution_stage_latency);

    rp_flush = make_read_port<bool>("BRANCH_2_ALL_FLUSH", Port::LATENCY);

    rps_bypass[0].command_port = make_read_port<BypassCommand<Register>>("DECODE_2_EXECUTE_SRC1_COMMAND", Port::LATENCY);
    rps_bypass[1].command_port = make_read_port<BypassCommand<Register>>("DECODE_2_EXECUTE_SRC2_COMMAND", Port::LATENCY);

    wp_bypass = make_write_port<InstructionOutput>("EXECUTE_2_EXECUTE_BYPASS", Port::BW);
    wp_long_arithmetic_bypass = make_write_port<InstructionOutput>("EXECUTE_COMPLEX_ALU_2_EXECUTE_BYPASS", Port::BW);

    rps_bypass[0].data_ports[0] = make_read_port<InstructionOutput>("EXECUTE_2_EXECUTE_BYPASS", Port::LATENCY);
    rps_bypass[1].data_ports[0] = make_read_port<InstructionOutput>("EXECUTE_2_EXECUTE_BYPASS", Port::LATENCY);

    rps_bypass[0].data_ports[1] = make_read_port<InstructionOutput>("EXECUTE_COMPLEX_ALU_2_EXECUTE_BYPASS", Port::LATENCY);
    rps_bypass[1].data_ports[1] = make_read_port<InstructionOutput>("EXECUTE_COMPLEX_ALU_2_EXECUTE_BYPASS", Port::LATENCY);

    rps_bypass[0].data_ports[2] = make_read_port<InstructionOutput>("MEMORY_2_EXECUTE_BYPASS", Port::LATENCY);
    rps_bypass[1].data_ports[2] = make_read_port<InstructionOutput>("MEMORY_2_EXECUTE_BYPASS", Port::LATENCY);

    rps_bypass[0].data_ports[3] = make_read_port<InstructionOutput>("WRITEBACK_2_EXECUTE_BYPASS", Port::LATENCY);
    rps_bypass[1].data_ports[3] = make_read_port<InstructionOutput>("WRITEBACK_2_EXECUTE_BYPASS", Port::LATENCY);

    rps_bypass[0].data_ports[4] = make_read_port<InstructionOutput>("BRANCH_2_EXECUTE_BYPASS", Port::LATENCY);
    rps_bypass[1].data_ports[4] = make_read_port<InstructionOutput>("BRANCH_2_EXECUTE_BYPASS", Port::LATENCY);
}

template <typename FuncInstr>
void Execute<FuncInstr>::clock( Cycle cycle)
{
    sout << "execute cycle " << std::dec << cycle << ": ";

    const bool is_flush = ( rp_flush->is_ready( cycle) && rp_flush->read( cycle))
                       || ( rp_trap->is_ready( cycle) && rp_trap->read( cycle));

    /* update information about mispredictions */
    clock_saved_flush();

    /* branch misprediction */
    if (is_flush)
    {
        save_flush();
        sout << "flush\n";
        return;
    }

    /* get the instruction from long ALU if it is ready */
    if ( rp_long_latency_execution_unit->is_ready( cycle))
    {
        auto instr = rp_long_latency_execution_unit->read( cycle);

        if ( has_flush_expired())
        {
            wp_long_arithmetic_bypass->write( instr.get_v_dst(), cycle);
            wp_writeback_datapath->write( instr, cycle);

            /* JSON dump of Execute-stage of executing operation */
            if (jsonout_enabled)
                (jsonout()) << ",\n\t{ \"type\": \"Event\", \"id\": " << instr.get_instruction_id() << ", \"cycle\": " << cycle << ", \"stage\": 2 }";
        }
    }


    /* check if there is something to process */
    if ( !rp_datapath->is_ready( cycle))
    {
        sout << "bubble\n";
        return;
    }

    auto instr = rp_datapath->read( cycle);

    auto src_index = 0;
    for ( auto& bypass_source : rps_bypass)
    {
        /* check whether bypassing is needed for a source register */
        if ( bypass_source.command_port->is_ready( cycle))
        {
            const auto bypass_direction = bypass_source.command_port->read( cycle).get_bypass_direction();
            auto& port = bypass_source.data_ports.at( bypass_direction);
            RegisterUInt data{};
            while ( port->is_ready( cycle))
                data = port->read( cycle)[0];
            instr.set_v_src( data, src_index);
        }
        ++src_index;
    }

    /* perform execution */
    instr.execute();

    /* log */
    sout << instr << std::endl;

    if ( instr.is_long_arithmetic()) 
    {
        wp_long_latency_execution_unit->write( std::move( instr), cycle);
    }
    else
    {
        /* bypass data */
        wp_bypass->write( instr.get_v_dst(), cycle);

        if ( instr.is_jump())
        {
            wp_branch_datapath->write( std::move( instr), cycle);
        }
        else if ( instr.is_mem_stage_required())
        {
            wp_mem_datapath->write( std::move( instr), cycle);
        }
        else
        {
            wp_writeback_datapath->write( std::move( instr), cycle);
        }

        /* JSON dump of Execute-stage of executing operation */
        if (jsonout_enabled)
            (jsonout()) << ",\n\t{ \"type\": \"Event\", \"id\": " << instr.get_instruction_id() << ", \"cycle\": " << cycle << ", \"stage\": 2 }";
    }
}


#include <mips/mips.h>
#include <risc_v/risc_v.h>

template class Execute<BaseMIPSInstr<uint32>>;
template class Execute<BaseMIPSInstr<uint64>>;
template class Execute<RISCVInstr<uint32>>;
template class Execute<RISCVInstr<uint64>>;
template class Execute<RISCVInstr<uint128>>;


