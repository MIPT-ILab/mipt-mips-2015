/**
 * log.h - logging class
 * includes methods to show warrnings and errors
 * Copyright 2017-2020 MIPT-MIPS team
 */
 
#include <infra/log.h>

#include <iostream>

Log::Log() : sout( std::cout), serr( std::cerr) { } 
Log::~Log() = default;

OStreamWrapper::OStreamWrapper( std::ostream& hide, std::ostream& expose)
    : ostream( hide)
    , buffer( hide.rdbuf())
{
    ostream.rdbuf( expose.rdbuf());
}

OStreamWrapper::~OStreamWrapper()
{
    ostream.rdbuf( buffer);
}


std::ofstream* visualizer_logger = new std::ofstream("logs.json", std::ofstream::out);
std::ofstream& Log::jsonout() { return *visualizer_logger; }

