/*
 * perf_sim.h - mips performance simulator
 * Copyright 2015-2018 MIPT-MIPS
 */

#ifndef PERF_SIM_H
#define PERF_SIM_H

#include "perf_instr.h"

#include <modules/branch/branch.h>
#include <modules/decode/decode.h>
#include <modules/execute/execute.h>
#include <modules/fetch/fetch.h>
#include <modules/mem/mem.h>
#include <modules/ports_instance.h>
#include <modules/writeback/writeback.h>
#include <simulator.h>

#include <chrono>

template <typename ISA>
class PerfSim : public CycleAccurateSimulator
{
public:
    using Register = typename ISA::Register;
    using RegisterUInt = typename ISA::RegisterUInt;
    explicit PerfSim( std::endian endian, std::string_view isa);
    Trap run( uint64 instrs_to_run) final;
    void set_target( const Target& target) final;
    void set_memory( std::shared_ptr<FuncMemory> memory) final;
    void set_kernel( std::shared_ptr<Kernel> k) final { writeback.set_kernel( k, get_isa()); }
    void disable_checker() final { writeback.disable_checker(); }
    void clock() final;
    void enable_driver_hooks() final { writeback.enable_driver_hooks(); }
    void set_writeback_bandwidth( uint32 wb_bandwidth) { decode.set_wb_bandwidth( wb_bandwidth);}
    int get_exit_code() const noexcept final { return writeback.get_exit_code(); }

    size_t sizeof_register() const final { return bytewidth<RegisterUInt>; }
    size_t max_cpu_register() const final { return Register::MAX_REG; }

    Addr get_pc() const final;
    
    uint64 read_cpu_register( size_t regno) const final { return read_register( Register::from_cpu_index( regno)); }
    uint64 read_gdb_register( size_t regno) const final;
    uint64 read_csr_register( std::string_view reg_name) const final { return read_register( Register::from_csr_name( reg_name)); }

    void write_cpu_register( size_t regno, uint64 value) final { write_register( Register::from_cpu_index( regno), value); }
    void write_gdb_register( size_t regno, uint64 value) final;
    void write_csr_register( std::string_view reg_name, uint64 value) final { write_register( Register::from_csr_name( reg_name), value); }

    // Rule of five
    PerfSim( const PerfSim&) = delete;
    PerfSim( PerfSim&&) = delete;
    PerfSim operator=( const PerfSim&) = delete;
    PerfSim operator=( PerfSim&&) = delete;
    ~PerfSim() override = default;
private:
    using FuncInstr = typename ISA::FuncInstr;
    using Instr = PerfInstr<FuncInstr>;

    Cycle curr_cycle = 0_cl;
    decltype( std::chrono::high_resolution_clock::now()) start_time = {};

    /* simulator units */
    RF<FuncInstr> rf;
    std::shared_ptr<FuncMemory> memory;
    const std::endian endian;

    Fetch<FuncInstr> fetch;
    Decode<FuncInstr> decode;
    Execute<FuncInstr> execute;
    Mem<FuncInstr> mem;
    Branch<FuncInstr> branch;
    Writeback<ISA> writeback;

    /* ports */
    ReadPort<Trap>* rp_halt = nullptr;

    void clock_tree( Cycle cycle);
    void dump_statistics() const;
    Trap current_trap = Trap(Trap::NO_TRAP);

    uint64 read_register( Register index) const { return narrow_cast<uint64>( rf.read( index)); }
    void write_register( Register index, uint64 value) { rf.write( index, narrow_cast<RegisterUInt>( value)); }

    /* default lines initialization */
    void start_dump_json();
    void stop_dump_json();
};

#endif
