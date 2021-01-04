/*
 * riscv64_test.cpp: RISC-V test generated from RISC-V testing repo
 * https://github.com/riscv/riscv-tests/tree/master/isa/rv64ui
 * Copyright 2019 MIPT-MIPS / MIPT-V
 */

#include <catch.hpp>
#include <memory/memory.h>
#include <risc_v/riscv_instr.h>
#include <infra/uint128.h>

template<typename T>
struct TestData {
    const T src1, src2, dst;

    TestData( T src1, T src2, T dst)
        : src1( std::move( src1))
        , src2( std::move( src2))
        , dst( std::move( dst))
    { }

    void make_test( std::string_view str, uint32 imm)
    {
        RISCVInstr<T> instr( str, imm);
        instr.set_v_src( src1, 0);
        instr.set_v_src( src2, 1);
        instr.execute();
        if constexpr ( std::is_same_v<T, uint128>) {
            // CATCH cannot handle 128 bit integers, let's check both parts sequentially
            CHECK( unpack_to<uint64>( instr.get_v_dst( 0))[0] == unpack_to<uint64>( dst)[0]);
            CHECK( unpack_to<uint64>( instr.get_v_dst( 0))[1] == unpack_to<uint64>( dst)[1]);
        }
        else {
            CHECK( instr.get_v_dst( 0) == dst);
        }
    }

    void make_test( std::string_view str)
    {
        make_test( str, 0);
    }
};

#define TEST_IMM_OP_HELPER( no, name, dst, src, imm, rv_version, RegisterUInt) \
    TEST_CASE( rv_version " autogenerated test " # name " " # no) { TestData<RegisterUInt>( (src), 0, (dst)).make_test( #name, (imm)); }

#define TEST_RR_OP_HELPER( no, name, dst, src1, src2, rv_version, RegisterUInt) \
    TEST_CASE( rv_version " autogenerated test " # name " " # no) { TestData<RegisterUInt>( (src1), (src2), (dst)).make_test( #name, 0); }


#define TEST_IMM_OP( no, name, dst, src, imm) TEST_IMM_OP_HELPER( no, name, dst, src, imm, "RISCV", typename Instr::RegisterUInt)
#define TEST_RV32_IMM_OP( no, name, dst, src, imm) TEST_IMM_OP_HELPER( no, name, dst, src, imm, "RISCV32", uint32)
#define TEST_RV64_IMM_OP( no, name, dst, src, imm) TEST_IMM_OP_HELPER( no, name, dst, src, imm, "RISCV64", uint64)
#define TEST_RV128_IMM_OP( no, name, dst, src, imm) TEST_IMM_OP_HELPER( no, name, dst, src, imm, "RISCV128", uint128)

#define TEST_IMM_SRC1_EQ_DEST( no, name, dst, src, imm)       TEST_IMM_OP( no, name, dst, src, imm)
#define TEST_IMM_SRC1_BYPASS( no, level, name, dst, src, imm) TEST_IMM_OP( no, name, dst, src, imm)
#define TEST_IMM_DEST_BYPASS( no, level, name, dst, src, imm) TEST_IMM_OP( no, name, dst, src, imm)
#define TEST_IMM_ZEROSRC1( no, name, dst, imm)                TEST_IMM_OP( no, name, dst, 0, imm)
#define TEST_IMM_ZERODEST( no, name, src, imm)

#define TEST_RR_OP( no, name, dst, src1, src2) TEST_RR_OP_HELPER( no, name, dst, src1, src2, "RISCV", typename Instr::RegisterUInt)
#define TEST_RV32_RR_OP( no, name, dst, src1, src2) TEST_RR_OP_HELPER( no, name, dst, src1, src2, "RISCV32", uint32)
#define TEST_RV64_RR_OP( no, name, dst, src1, src2) TEST_RR_OP_HELPER( no, name, dst, src1, src2, "RISCV64", uint64)
#define TEST_RV128_RR_OP( no, name, dst, src1, src2) TEST_RR_OP_HELPER( no, name, dst, src1, src2, "RISCV128", uint128)

#define TEST_RR_SRC1_EQ_DEST( no, name, dst, src1, src2) TEST_RR_OP( no, name, dst, src1, src2)
#define TEST_RR_SRC2_EQ_DEST( no, name, dst, src1, src2) TEST_RR_OP( no, name, dst, src1, src2)
#define TEST_RR_SRC12_EQ_DEST( no, name, dst, src)       TEST_RR_OP( no, name, dst, src,  src)
#define TEST_RR_ZEROSRC1( no, name, dst, src2)           TEST_RR_OP( no, name, dst, 0,    src2)
#define TEST_RR_ZEROSRC2( no, name, dst, src1)           TEST_RR_OP( no, name, dst, src1, 0)
#define TEST_RR_ZEROSRC12( no, name, dst)                TEST_RR_OP( no, name, dst, 0, 0)
#define TEST_RR_ZERODEST( no, name, src1, src2)
#define TEST_RR_SRC12_BYPASS( no, level1, level2, name, dst, src1, src2) TEST_RR_OP( no, name, dst, src1, src2)
#define TEST_RR_SRC21_BYPASS( no, level1, level2, name, dst, src1, src2) TEST_RR_OP( no, name, dst, src1, src2)
#define TEST_RR_DEST_BYPASS( no, level, name, dst, src1, src2)           TEST_RR_OP( no, name, dst, src1, src2)

#define TEST_BR2_OP_TAKEN( no, name, src1, src2) \
    TEST_CASE( "RISCV autogenerated test " # name " " # no) { \
        Instr instr( #name, 0x100); \
        instr.set_v_src( narrow_cast<uint64>( src1), 0); \
        instr.set_v_src( narrow_cast<uint64>( src2), 1); \
        instr.execute(); \
        CHECK( instr.get_new_PC() == instr.get_PC() + 0x100); \
    }

#define TEST_BR2_OP_NOTTAKEN( no, name, src1, src2) \
    TEST_CASE( "RISCV autogenerated test " # name " " # no) { \
        Instr instr( #name, 0x100); \
        instr.set_v_src( narrow_cast<uint64>( src1), 0); \
        instr.set_v_src( narrow_cast<uint64>( src2), 1); \
        instr.execute(); \
        CHECK( instr.get_new_PC() == instr.get_PC() + 4); \
    }

#define TEST_BR2_SRC12_BYPASS( no, level1, level2, name, src1, src2) TEST_BR2_OP_NOTTAKEN( no, name, src1, src2)

#define TEST_JALR_SRC1_BYPASS( no, level, name)
#define TEST_SRLI(n, v, a) TEST_IMM_OP(n, srli, ((v) & ( all_ones<Instr::RegisterUInt>())) >> size_t{a}, v, a)
#define TEST_SRL(n, v, a)  TEST_RR_OP(n,  srl, ((v) & ( all_ones<Instr::RegisterUInt>())) >> size_t{a}, v, a)

#define TEST_ST_OP( no, load_inst, store_inst, data, imm, address) \
    TEST_CASE( "RISCV autogenerated test " # store_inst " " # no) { \
        Instr load( #load_inst, narrow_cast<uint32>( imm)); \
        Instr store( #store_inst, narrow_cast<uint32>( imm)); \
        store.set_v_src( data, 1); \
        auto memory = FuncMemory::create_plain_memory(13); \
        for (auto* instr : { &load, &store }) { \
            instr->set_v_src( 0x1000, 0); \
            instr->execute(); \
        } \
        memory->load_store( &store); \
        memory->load_store( &load); \
        CHECK( load.get_v_dst( 0) == ( data)); \
    }

#define TEST_ST_SRC12_BYPASS( n, level1, level2, load, store, data, immediate, address) TEST_ST_OP( n, load, store, data, immediate, address)
#define TEST_ST_SRC21_BYPASS( n, level1, level2, load, store, data, immediate, address) TEST_ST_OP( n, load, store, data, immediate, address)
        
#define TEST_DISASM( instr_bytes, output_str, RegisterUInt) \
    CHECK( RISCVInstr<RegisterUInt>(instr_bytes).get_disasm() == ( output_str) )

#define TEST_RV32_DISASM(instr_bytes, output_str) TEST_DISASM( instr_bytes, output_str, uint32)
#define TEST_RV64_DISASM(instr_bytes, output_str) TEST_DISASM( instr_bytes, output_str, uint64)
#define TEST_RV128_DISASM(instr_bytes, output_str) TEST_DISASM( instr_bytes, output_str, uint128)
