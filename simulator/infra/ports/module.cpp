/**
 * module.cpp - module template
 * @author Pavel Kryukov
 * Copyright 2019 MIPT-V team
 */

#include "module.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

Module::Module( Module* parent, std::string name)
    : parent( parent), name( std::move( name))
{
    if ( parent != nullptr)
        parent->add_child( this);
}

// NOLINTNEXTLINE(misc-no-recursion) Recursive, but must be finite
void Module::force_enable_logging()
{  
    sout.enable();
    for (const auto& c : children)
        c->force_enable_logging();
}

// NOLINTNEXTLINE(misc-no-recursion) Recursive, but must be finite
void Module::force_disable_logging()
{  
    sout.disable();
    for (const auto& c : children)
        c->force_disable_logging();
}

// NOLINTNEXTLINE(misc-no-recursion) Recursive, but must be finite
void Module::enable_logging_impl( const std::unordered_set<std::string>& names)
{
    if ( names.count( name) != 0)
        force_enable_logging();
    else if ( names.count( '!' + name) != 0)
        force_disable_logging();;

    for ( const auto& c : children)
        c->enable_logging_impl( names);
}

// enable logging function implementation
void Module::enable_json_logging(bool key) {
    jsonout_enabled = key;
    for (const auto& c : children)
        c->enable_json_logging(key);
}

pt::ptree Module::write_ports_dumping() const
{
    pt::ptree result;
    for ( const auto& p : write_ports)
        result.put( p->get_key(), "");
    return result;
}

pt::ptree Module::read_ports_dumping() const
{
    pt::ptree result;
    for ( const auto& p : read_ports) 
        result.put( p->get_key(), "");
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion) Recursive, but must be finite
void Module::module_dumping( pt::ptree* modules) const
{
    pt::ptree module;
    module.add_child( "write_ports", write_ports_dumping());
    module.add_child( "read_ports", read_ports_dumping());
    modules->add_child( name, module);
    for ( const auto& c : children)
        c->module_dumping( modules);
}

void Module::modulemap_dumping( pt::ptree* modulemap) const
{
    pt::ptree c_modulemap;
    for ( const auto& c : children) 
        c->modulemap_dumping( &c_modulemap);
    modulemap->add_child( name, c_modulemap);
}

// NOLINTNEXTLINE(misc-no-recursion) Recursive, but must be finite
pt::ptree Module::portmap_dumping() const
{
    return parent->portmap_dumping();
}

pt::ptree Module::topology_dumping_impl() const
{
    pt::ptree topology;
    pt::ptree modules;
    pt::ptree modulemap;

    module_dumping( &modules);
    modulemap_dumping( &modulemap);

    topology.add_child( "modules", modules);
    topology.add_child( "portmap", portmap_dumping());
    topology.add_child( "modulemap", modulemap);
    return topology;
}

pt::ptree Root::portmap_dumping() const
{
    return portmap->dump();
}

void Root::enable_logging( const std::string& values)
{
    std::unordered_set<std::string> tokens;
    boost::split( tokens, values, boost::is_any_of( std::string(",")));
    enable_logging_impl( tokens);
}

void Root::topology_dumping( bool dump, const std::string& filename)
{
    if ( !dump)
        return;

    pt::ptree topology = topology_dumping_impl();
    pt::write_json( filename, topology);
    sout << std::endl << "Module topology dumped into " + filename << std::endl;
}
