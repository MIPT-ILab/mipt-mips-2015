/**
 * operation.h - list of basic operation types
 * @author Pavel Kryukov pavel.kryukov@phystech.edu
 * Copyright 2019 MIPT-MIPS
 */
 
#ifndef OPERATION_H
#define OPERATION_H

#include <func_sim/traps/trap.h>
#include <infra/macro.h>
#include <infra/types.h>

#include <array>
#include <bitset>
#include <cassert>
#include <sstream>
#include <string_view>

#define MAX_SRC_NUM 3
#define MAX_DST_NUM 2

enum OperationType : uint8
{
    OUT_ARITHM,
    OUT_DIVMULT,
    OUT_R_ACCUM,
    OUT_R_CONDM,
    OUT_R_JUMP,
    OUT_SYSCALL,
    OUT_BREAK,
    OUT_R_SUBTR,
    OUT_BRANCH,
    OUT_BRANCH_LIKELY,
    OUT_TRAP,
    OUT_LOAD,
    OUT_LOADU,
    OUT_PARTIAL_LOAD,
    OUT_STORE,
    OUT_J_JUMP,
    OUT_J_SPECIAL,
    OUT_FPU,
    OUT_UNKNOWN
};

enum class Imm : uint8
{
    NO, SHIFT,
    LOGIC, ARITH, TRAP, ADDR,
    JUMP, JUMP_REL
};

template<typename T>
std::string print_immediate( Imm type, T value)
{
    std::ostringstream oss;
    switch ( type)
    {
    case Imm::ADDR:     oss << ", 0x" << std::hex << narrow_cast<uint16>(value) << std::dec; break;
    case Imm::LOGIC:    oss << ", 0x" << std::hex << value << std::dec; break;
    case Imm::JUMP:     oss <<  " 0x" << std::hex << value << std::dec; break;
    case Imm::JUMP_REL: oss <<    " " << std::dec << narrow_cast<int16>(value); break;
    case Imm::TRAP:     oss << ", 0x" << std::hex << narrow_cast<int16>(value) << std::dec; break;
    case Imm::ARITH:    oss << ", "   << std::dec << narrow_cast<int16>(value); break;
    case Imm::SHIFT:    oss << ", "   << std::dec << value; break;
    case Imm::NO:       break;
    }
    return std::move( oss).str();
}

class Operation
{
public:
    Operation(Addr pc, Addr new_pc) : PC(pc), new_PC(new_pc) { }

    void set_type( OperationType type) { operation = type; }

    //target is known at ID stage and always taken
    bool is_direct_jump() const { return operation == OUT_J_JUMP; }

    //target is known at ID stage but if branch is taken or not is known only at EXE stage
    bool is_common_branch() const { return operation == OUT_BRANCH; }

    //target is known at ID stage; likely to be taken
    bool is_likely_branch() const { return operation == OUT_BRANCH_LIKELY; }

    bool is_branch() const { return is_common_branch() || is_likely_branch(); }

    // target is known only at EXE stage
    bool is_indirect_jump() const { return operation == OUT_R_JUMP; }

    bool is_jump() const { return this->is_direct_jump()
                               || this->is_branch()
                               || this->is_indirect_jump(); }

    bool is_taken() const
    {
        return ( this->is_direct_jump() ) || ( this->is_indirect_jump() ) || is_taken_branch;
    }

    bool is_partial_load()  const { return operation == OUT_PARTIAL_LOAD; }
    bool is_unsigned_load() const { return operation == OUT_LOADU; }
    bool is_signed_load()   const { return operation == OUT_LOAD; }
    bool is_load() const { return is_unsigned_load() || is_signed_load() || is_partial_load(); }

    int8 get_accumulation_type() const
    {
        return (operation == OUT_R_ACCUM) ? 1 : (operation == OUT_R_SUBTR) ? -1 : 0;
    }
    Trap trap_type() const { return trap; }

    bool is_halt() const { return trap_type() == Trap::HALT; }
    bool is_conditional_move() const { return operation == OUT_R_CONDM; }
    bool is_divmult() const { return operation == OUT_DIVMULT || get_accumulation_type() != 0; }

    bool is_explicit_trap() const { return operation == OUT_TRAP; }
    bool has_trap() const { return trap_type() != Trap::NO_TRAP; }
    void set_trap( Trap value) { trap = value; }
    bool is_store() const { return operation == OUT_STORE; }

    auto get_mem_addr() const { return mem_addr; }
    auto get_mem_size() const { return mem_size; }
    auto get_PC() const { return PC; }

    void set_sequence_id( uint64 id) { sequence_id = id; }
    auto get_sequence_id() const { return sequence_id; }

    // instruction_id initialization and return - functions
    void set_instruction_id(uint64 id) { instruction_id = id; }
    auto get_instruction_id() const { return instruction_id; }

    auto get_delayed_slots() const { return delayed_slots; }
    Addr get_decoded_target() const { return target; }
    auto get_new_PC() const { return new_PC; }

protected:
    std::string_view opname = {};
    Trap trap = Trap(Trap::NO_TRAP);

    Addr mem_addr = NO_VAL32;
    uint32 mem_size = NO_VAL32;
    uint32 v_imm = NO_VAL32;
    Imm imm_print_type = Imm::NO;
    uint8 delayed_slots = 0;

    // convert this to bitset
    bool is_taken_branch = false; // actual result
    std::bitset<MAX_DST_NUM> print_dst;
    std::bitset<MAX_SRC_NUM> print_src;

    const Addr PC = NO_VAL32;
    Addr new_PC = NO_VAL32;
    Addr target = NO_VAL32;

private:
    OperationType operation = OUT_UNKNOWN;
    uint64 sequence_id = NO_VAL64;
    // instruction_id initialization
    uint64 instruction_id = NO_VAL64;
};

template <typename I>
struct ALU;

template <typename I>
struct RISCVMultALU;

template <typename I>
struct MIPSMultALU;

template<typename T>
class Datapath : public Operation
{
public:
    friend ALU<Datapath>;
    friend RISCVMultALU<Datapath>;
    friend MIPSMultALU<Datapath>;

    using Execute = void (*)(Datapath*);
    using RegisterUInt = T;
    using RegisterSInt = sign_t<RegisterUInt>;

    void set_v_src( const T& value, size_t index) { v_src.at( index) = value; }
    T get_v_src( size_t index) const { return v_src.at( index); }

    const std::array<T, MAX_DST_NUM>& get_v_dst() const { return v_dst; }
    void set_v_dst( const T& value, size_t index) { v_dst.at( index) = value; }
    T get_v_dst( size_t index) const { return get_v_dst().at( index); }

    T get_mask() const { return mask; }
    T get_v_imm() const { return v_imm; }
    T sign_extension_for_load( const T& value) const { return sign_extension(value, 8 * get_mem_size()); }

    bool has_memory_address() const { return ( is_load() || is_store()) && complete; }
    bool is_dst_complete() const { return is_load() ? memory_complete : complete; }

    void load( const T& value);
    void execute();

protected:
    Datapath(Addr pc, Addr new_pc) : Operation(pc, new_pc) {}

    std::array<T, MAX_SRC_NUM> v_src = { NO_VAL32, NO_VAL32, NO_VAL32 };
    std::array<T, MAX_DST_NUM> v_dst = { NO_VAL32, NO_VAL32 };
    T v_imm = NO_VAL32;
    T mask = all_ones<T>();

    Execute executor;

private:
    bool complete   = false;
    bool memory_complete = false;
};

template<typename T>
void Datapath<T>::execute()
{
    executor(this);
    complete = true;
}

template<typename T>
void Datapath<T>::load( const T& value)
{
    memory_complete = true;
    assert( is_load());
    set_v_dst( is_unsigned_load() ? value : sign_extension_for_load(value), 0);
}

template<typename T, typename R>
class BaseInstruction : public Datapath<T>
{
public:
    using MyDatapath = Datapath<T>;
    using Register = R;
    using RegisterUInt = T;
    R get_src( size_t index) const { return src.at( index); }
    R get_dst( size_t index) const { return dst.at( index); }
    void set_src( R reg, size_t index) { src.at( index) = reg; }
    void set_dst( R reg, size_t index) { dst.at( index) = reg; }

    std::ostream& dump_content( std::ostream& out, const std::string& disasm) const;

protected:
    BaseInstruction(Addr pc, Addr new_pc) : Datapath<T>(pc, new_pc) { }
    std::string generate_disasm() const;

    std::array<R, MAX_SRC_NUM> src = { R::zero(), R::zero(), R::zero() };
    std::array<R, MAX_DST_NUM> dst = { R::zero(), R::zero() };
};

template<typename T, typename R>
std::string BaseInstruction<T, R>::generate_disasm() const
{
    std::ostringstream oss;
    oss << this->opname;

    if ( this->imm_print_type == Imm::ADDR)
    {
        oss << " $" << (this->print_dst.test( 0) ? get_dst( 0) : get_src( 1))
            << print_immediate( Imm::ADDR, this->get_v_imm())
            << "($" << get_src( 0) << ")" << std::dec;
        return std::move( oss).str();
    }

    for ( size_t i = 0; i < MAX_DST_NUM; i++)
        if ( this->print_dst.test( i))
            oss << ( i > 0 ? ", $" : " $") << get_dst( i);
    if ( this->print_src.test( 0) && this->print_dst.any())
        oss <<  ",";
    for ( size_t i = 0; i < MAX_SRC_NUM; i++)
        if ( this->print_src.test( i))
            oss << ( i > 0 ? ", $" : " $") << get_src( i);

    oss << print_immediate( this->imm_print_type, this->get_v_imm());
    return std::move( oss).str();
}

template<typename T, typename R>
std::ostream& BaseInstruction<T, R>::dump_content( std::ostream& out, const std::string& disasm) const
{
    if ( this->PC != 0)
        out << std::hex << "0x" << this->PC << ": ";

    out << "{" << std::dec << this->get_sequence_id() << "}\t" << disasm << "\t [";
    bool has_ma = this->has_memory_address();
    if ( has_ma)
        out << " $ma = 0x" << std::hex << this->get_mem_addr();

    if ( this->is_dst_complete())
    {
        bool has_dst = !get_dst( 0).is_zero() && this->get_mask();
        if ( has_ma && has_dst)
            out << ",";

        if ( has_dst)
            out << " $" << get_dst( 0) << " = 0x" << std::hex << (this->get_v_dst( 0) & this->get_mask());

        if ( !get_dst( 1).is_zero()) {
            if ( has_dst)
                out << ",";
            out << " $" << get_dst( 1) << " = 0x" << this->get_v_dst( 1);
        }
    }
    out << " ]";
    if ( this->trap != Trap::NO_TRAP)
        out << "\t " << this->trap;

    out << std::dec;
    return out;
}


#endif // OPERATION_H
 
