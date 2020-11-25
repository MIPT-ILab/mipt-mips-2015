/**
 * timing.h - Implementation of safe timing primitive types
 *
 * @author Denis Los
 * Copyright 2018-2020 MIPT-MIPS project
 */
 
#include "timing.h"

#include <iostream>

std::ostream& operator<<( std::ostream& os, const Cycle& cycle)
{
    return os << cycle.value;
}

std::istream& operator>>( std::istream& is, Cycle& cycle)
{
    return is >> cycle.value;
}

std::ostream& operator<<( std::ostream& os, const Latency& latency)
{
    return os << latency.value;
}

std::istream& operator>>( std::istream& is, Latency& latency)
{
    return is >> latency.value;
}
