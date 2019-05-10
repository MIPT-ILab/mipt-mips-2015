/*
 * Unit testing for extremely simple simulator
 * Copyright 2018 MIPT-MIPS
 */

#include "../func_sim.h"


#include <catch.hpp>

#include <kernel/kernel.h>
#include <kernel/mars/mars_kernel.h>
#include <memory/elf/elf_loader.h>
#include <memory/memory.h>
#include <mips/mips_register/mips_register.h>
#include <simulator.h>

static const std::string valid_elf_file = TEST_PATH "/tt.core.universal.out";
static const std::string smc_code = TEST_PATH "/smc.out";

TEST_CASE( "Process_Wrong_Args_Of_Constr: Func_Sim_init")
{
    // Just call a constructor
    CHECK_NOTHROW( Simulator::create_functional_simulator("mips32") );
    CHECK_THROWS_AS( Simulator::create_functional_simulator("pdp11"), InvalidISA);
}

TEST_CASE( "Bad driver")
{
    CHECK_THROWS_AS( Simulator::create_functional_simulator("mips32")->setup_trap_handler("abracadabra"), IncorrectDriver);
}

static auto run_over_empty_memory( const std::string& isa)
{
    auto sim = Simulator::create_functional_simulator( isa);
    sim->setup_trap_handler( "stop");
    sim->set_memory( FuncMemory::create_hierarchied_memory());
    return sim->run( 30);
}

TEST_CASE( "FuncSim: create empty memory and get lost")
{
    CHECK_THROWS_AS( run_over_empty_memory("mips32"), BearingLost);
    CHECK( run_over_empty_memory("riscv32") == Trap::UNKNOWN_INSTRUCTION);
    CHECK( run_over_empty_memory("riscv128") == Trap::UNKNOWN_INSTRUCTION);
}

TEST_CASE( "FuncSim: get lost without pc")
{
    auto m   = FuncMemory::create_hierarchied_memory();
    auto sim = Simulator::create_functional_simulator("mips32");
    sim->set_memory( m);
    ElfLoader( valid_elf_file).load_to( m.get());
    CHECK_THROWS_AS( sim->run_no_limit(), BearingLost);
}

TEST_CASE( "Process_Wrong_Args_Of_Constr: Func_Sim_init_and_load")
{
    // Call constructor and init
    auto sim = Simulator::create_functional_simulator("mips32");
    auto mem = FuncMemory::create_hierarchied_memory();
    sim->set_memory( mem);
    ElfLoader elf( valid_elf_file);
    elf.load_to( mem.get());
    CHECK_NOTHROW( sim->set_pc( elf.get_startPC()) );
}

TEST_CASE( "Make_A_Step: Func_Sim")
{
    auto sim = Simulator::create_functional_simulator("mips32");
    auto mem = FuncMemory::create_hierarchied_memory();
    sim->set_memory( mem);
    ElfLoader elf( valid_elf_file);
    elf.load_to( mem.get());
    sim->set_pc( elf.get_startPC());
    sim->init_checker();

    CHECK( sim->get_pc() == elf.get_startPC());
    sim->run( 1);
    CHECK( sim->get_pc() == elf.get_startPC() + 4);
}

TEST_CASE( "Run one instruction: Func_Sim")
{
    auto sim = Simulator::create_functional_simulator("mips32");
    auto mem = FuncMemory::create_hierarchied_memory();
    sim->set_memory( mem);
    ElfLoader elf( smc_code);
    elf.load_to( mem.get());
    sim->set_pc( elf.get_startPC());

    CHECK( sim->run( 1) == Trap::NO_TRAP);
}

TEST_CASE( "FuncSim: Register R/W")
{
    auto sim = Simulator::create_functional_simulator("mips32");

    /* Signed */
    sim->write_cpu_register( 1, narrow_cast<uint64>( -1337));
    CHECK( narrow_cast<int32>( sim->read_cpu_register( 1)) == -1337 );
    /* Unsigned */
    sim->write_cpu_register( 1, uint64{ MAX_VAL32});
    CHECK( sim->read_cpu_register( 1) == MAX_VAL32 );
}

TEST_CASE( "FuncSim: GDB Register R/W")
{
    auto sim = Simulator::create_functional_simulator("mips32");

    sim->write_gdb_register( 1, uint64{ MAX_VAL32});
    CHECK( sim->read_gdb_register( 1) == MAX_VAL32 );
    CHECK( sim->read_gdb_register( 0) == 0 );

    sim->write_gdb_register( 37, 100500);
    CHECK( sim->read_gdb_register( 37) == 100500);
    CHECK( sim->get_pc() == 100500);
}

TEST_CASE( "FuncSim: Register size")
{
    CHECK( Simulator::create_functional_simulator("mips32")->sizeof_register() == bytewidth<uint32>);
    CHECK( Simulator::create_functional_simulator("mips64")->sizeof_register() == bytewidth<uint64>);
}

TEST_CASE( "Run_SMC_trace: Func_Sim")
{
    auto sim = Simulator::create_functional_simulator("mips32");
    auto mem = FuncMemory::create_hierarchied_memory();
    sim->set_memory( mem);
    sim->setup_trap_handler( "stop_on_halt");
    ElfLoader elf( smc_code);
    elf.load_to( mem.get());
    sim->set_pc( elf.get_startPC());

    CHECK_NOTHROW( sim->run_no_limit() );
}

TEST_CASE( "Torture_Test: MIPS32 calls without kernel")
{
    bool log = false;
    auto sim = Simulator::create_functional_simulator("mips32", log);
    auto mem = FuncMemory::create_hierarchied_memory( 36);
    sim->set_memory( mem);

    ElfLoader elf( valid_elf_file);
    elf.load_to( mem.get());
    auto start_pc = elf.get_startPC();
    sim->set_pc( start_pc);

    CHECK_THROWS_AS( sim->run( 10000), BearingLost);
    CHECK( sim->get_pc() >= 0x8'0000'0180);
    CHECK( sim->read_cpu_register( MIPSRegister::cause().to_rf_index()) != 0);
    auto epc = sim->read_cpu_register( MIPSRegister::epc().to_rf_index());
    CHECK( epc > start_pc);
    CHECK( epc < start_pc + 0x1000'000);
}

static auto get_simulator_with_test( const std::string& isa, const std::string& test, const std::string& trap_options)
{
    bool log = false;
    auto sim = Simulator::create_functional_simulator(isa, log);
    auto mem = FuncMemory::create_hierarchied_memory();
    sim->set_memory( mem);
    sim->setup_trap_handler( trap_options);

    ElfLoader elf( test);
    elf.load_to( mem.get());

    auto kernel = create_mars_kernel();
    kernel->connect_memory( mem);
    kernel->set_simulator( sim);
    sim->set_kernel( kernel);

    sim->set_pc( elf.get_startPC());
    return sim;
}

TEST_CASE( "Torture_Test: Stop on trap")
{
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "stop")->run( 1) == Trap::NO_TRAP );
    auto trap = get_simulator_with_test("mips32", valid_elf_file, "stop")->run( 10000);
    CHECK( trap != Trap::NO_TRAP );
    CHECK( trap != Trap::HALT );
}

TEST_CASE( "Torture_Test: Stop on halt")
{
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "stop_on_halt")->run( 1)     == Trap::NO_TRAP );
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "stop_on_halt")->run( 10000) == Trap::HALT );
}

TEST_CASE( "Torture_Test: Ignore traps ")
{
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "ignore")->run( 1)  == Trap::NO_TRAP );
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "stop"  )->run( 10) != Trap::NO_TRAP);
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "ignore")->run( 10) == Trap::NO_TRAP);
}

TEST_CASE( "Torture_Test: Critical traps ")
{
    CHECK_NOTHROW( get_simulator_with_test("mips32", valid_elf_file, "critical")->run( 1) );
    CHECK_THROWS_AS( get_simulator_with_test("mips32", valid_elf_file, "critical")->run( 10000), std::runtime_error);
}

TEST_CASE( "Torture_Test: MIPS32 calls ")
{
    CHECK( get_simulator_with_test("mips32", valid_elf_file, "")->run( 10000) == Trap::HALT );
}

TEST_CASE( "Torture_Test: integration")
{
    CHECK( get_simulator_with_test("mars",    valid_elf_file, "stop_on_halt")->run_no_limit() == Trap::HALT );
    CHECK( get_simulator_with_test("mips32",  TEST_PATH "/tt.core.universal_reorder.out", "stop_on_halt")->run_no_limit() == Trap::HALT );
    CHECK( get_simulator_with_test("riscv32", RISCV_TEST_PATH "/isa/rv32ui-p-simple", "stop_on_halt")->run_no_limit() == Trap::HALT );
    CHECK( get_simulator_with_test("riscv64", RISCV_TEST_PATH "/isa/rv64ui-p-simple", "stop_on_halt")->run_no_limit() == Trap::HALT );
}
