/**
 * driver.cpp - exception handler
 * @author Vsevolod Pukhov, Pavel Kryukov
 * Copyright 2019 MIPT-MIPS
 */

#include "driver.h"

#include <boost/tokenizer.hpp>

class DriverImpl : public Driver
{
public:
    DriverImpl( const std::string& mode);
    Trap handle_trap( Trap trap) const final;
private:
    enum class HandleTrapMode : uint8
    {
        STOP,
        STOP_ON_HALT,
        IGNORE,
    } handle_trap_mode = HandleTrapMode::STOP_ON_HALT;

    bool handle_trap_critical = false;
    bool handle_trap_verbose = false;
};

DriverImpl::DriverImpl( const std::string& mode)
{
    for ( const auto& e : boost::tokenizer( mode, boost::char_separator(",")))
        if ( e == "stop")
            handle_trap_mode = HandleTrapMode::STOP;
        else if ( e == "stop_on_halt")
            handle_trap_mode = HandleTrapMode::STOP_ON_HALT;
        else if ( e == "ignore")
            handle_trap_mode = HandleTrapMode::IGNORE;
        else if ( e == "critical")
            handle_trap_critical = true;
        else if ( e == "verbose")
            handle_trap_verbose = true;
}

std::unique_ptr<Driver> Driver::construct( const std::string& mode, Simulator* /* sim */)
{
    return std::make_unique<DriverImpl>( mode);
}

Trap DriverImpl::handle_trap( Trap trap) const
{
    if ( trap == Trap::NO_TRAP)
        return trap;

    if ( handle_trap_verbose)
        std::cout << "\tFuncSim trap: " << trap << std::endl;

    if ( handle_trap_critical)
        throw std::runtime_error( "critical trap");

    switch ( handle_trap_mode)
    {
    case HandleTrapMode::STOP_ON_HALT: return trap == Trap::HALT ? trap : Trap(Trap::NO_TRAP);
    case HandleTrapMode::IGNORE: return Trap(Trap::NO_TRAP);
    default: return trap;
    }
}
