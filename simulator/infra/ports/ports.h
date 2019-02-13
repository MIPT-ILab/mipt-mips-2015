/**
 * ports.h - template for simulation of ports.
 * @author Pavel Kryukov
 * Copyright 2017-2019 MIPT-MIPS team
 */

#ifndef PORTS_H
#define PORTS_H

#include "../exception.h"
#include "../log.h"
#include "../types.h"
#include "port_queue/port_queue.h"
#include "timing.h"

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct PortError final : Exception {
    explicit PortError( const std::string& msg)
        : Exception("Port error", msg)
    { }
};

class PortMap : public Log
{
private:
    struct Cluster
    {
        class BasicWritePort* writer = nullptr;
        std::vector<class BasicReadPort*> readers = {};
    };

    std::unordered_map<std::string, Cluster> map = { };
    PortMap() noexcept;
public:
    void add_port( BasicWritePort* port);
    void add_port( BasicReadPort* port);

    static PortMap& get_instance();

    void init() const;
    void clean_up( Cycle cycle);
    void destroy();
};

class Port : public Log
{
    const std::string _key;
protected:
    PortMap& portMap = PortMap::get_instance();
    explicit Port( std::string key);
public:
    const std::string& get_key() const { return _key; }
};

class BasicReadPort : public Port
{
    const Latency _latency;
protected:
    BasicReadPort( const std::string& key, Latency latency);
    auto get_latency() const noexcept { return _latency; }
};

class BasicWritePort : public Port
{
    friend class PortMap;
    const uint32 _fanout;
    Cycle _lastCycle = 0_cl;
    uint32 _writeCounter = 0;
    uint32 initialized_bandwidth = 0;
    const uint32 installed_bandwidth;

    virtual void init( const std::vector<BasicReadPort*>& readers) = 0;
    virtual void clean_up( Cycle cycle) noexcept = 0;
protected:
    BasicWritePort( const std::string& key, uint32 bandwidth, uint32 fanout);
    void base_init( const std::vector<BasicReadPort*>& readers);
    void prepare_to_write( Cycle cycle);
public:
    auto get_fanout() const noexcept { return _fanout; }
    auto get_bandwidth() const noexcept { return initialized_bandwidth; }
};

// Make it inline since it is used often
inline void BasicWritePort::prepare_to_write( Cycle cycle)
{
    _writeCounter = _lastCycle == cycle ? _writeCounter + 1 : 0;
    _lastCycle = cycle;

    if ( _writeCounter > get_bandwidth())
        throw PortError( get_key() + " port is overloaded by bandwidth");
}

template<class T> class ReadPort;
    
template<class T> class WritePort : public BasicWritePort
{
    std::vector<ReadPort<T>*> destinations = {};
    void add_port( BasicReadPort* r);
    void init( const std::vector<BasicReadPort*>& readers) final;
    void clean_up( Cycle cycle) noexcept final;
    ReadPort<T>* port_cast( Port* p) const;
    void basic_write( T&& what, Cycle cycle) noexcept( std::is_nothrow_copy_constructible<T>::value);
public:
    WritePort<T>( const std::string& key, uint32 bandwidth, uint32 fanout)
        : BasicWritePort( key, bandwidth, fanout)
    { }

    void write( T&& what, Cycle cycle)
    {
        prepare_to_write( cycle);
        basic_write( std::forward<T>( what), cycle);
    }

    void write( const T& what, Cycle cycle)
    {
        prepare_to_write( cycle);
        basic_write( std::move( T( what)), cycle);
    }
};

template<class T> class ReadPort : public BasicReadPort
{
    friend class WritePort<T>;
private:
    PortQueue<std::pair<T, Cycle>> queue;

    void emplaceData( T&& what, Cycle cycle)
        noexcept( std::is_nothrow_copy_constructible<T>::value)
    {
        queue.emplace( std::move( what), cycle + get_latency());
    }

    void init( uint32 bandwidth)
    {
        // +1 to handle reads-after-writes
        queue.resize( ( get_latency().to_size_t() + 1) * bandwidth);
    }

    void clean_up( Cycle cycle) noexcept;

public:
    ReadPort<T>( const std::string& key, Latency latency) : BasicReadPort( key, latency) { }

    bool is_ready( Cycle cycle) const noexcept
    {
        return !queue.empty() && std::get<Cycle>(queue.front()) == cycle;
    }

    T read( Cycle cycle);
};

template<class T>
void WritePort<T>::clean_up( Cycle cycle) noexcept
{
    for ( const auto& reader : destinations)
        reader->clean_up( cycle);
}

template<class T>
void WritePort<T>::basic_write( T&& what, Cycle cycle)
    noexcept( std::is_nothrow_copy_constructible<T>::value)
{
    // Copy data to all ports except first one
    auto it = std::next( destinations.begin());
    for ( ; it != destinations.end(); ++it)
        (*it)->emplaceData( std::move( T( what)), cycle); // Force copy ctor

    // Move data to the first port
    destinations.front()->emplaceData( std::forward<T>( what), cycle);
}

template<class T>
ReadPort<T>* WritePort<T>::port_cast( Port* p) const try
{
    return dynamic_cast<ReadPort<T>*>( p);
}
catch ( const std::bad_cast&)
{
    throw PortError( get_key() + " has type mismatch between write and read ports");
}
    
template<class T>
void WritePort<T>::add_port( BasicReadPort* r)
{
    auto reader = port_cast( r);
    destinations.emplace_back( reader);
    reader->init( get_bandwidth());    
}

template<class T>
void WritePort<T>::init( const std::vector<BasicReadPort*>& readers)
{
    base_init( readers);
    destinations.reserve( readers.size());
    for (const auto& r : readers)
        add_port( r);
}

template<class T> T ReadPort<T>::read( Cycle cycle)
{
    if ( !is_ready( cycle))
        throw PortError( get_key() + " ReadPort was not ready for read at cycle=" + cycle.to_string());

    T tmp( std::move( std::get<T>(queue.front())));
    queue.pop();
    return tmp;
}

template<class T> void ReadPort<T>::clean_up( Cycle cycle) noexcept
{
    while ( !queue.empty() && std::get<Cycle>(queue.front()) < cycle)
        queue.pop();
}

// External methods
template<typename T>
decltype(auto) make_write_port( std::string key, uint32 bandwidth, uint32 fanout) 
{
    return std::make_unique<WritePort<T>>( std::move(key), bandwidth, fanout);
}

template<typename T>
auto make_read_port( std::string key, Latency latency)
{
    return std::make_unique<ReadPort<T>>( std::move(key), latency);
}

static constexpr const Latency PORT_LATENCY = 1_lt;
static constexpr const Latency PORT_LONG_LATENCY = 30_lt;
static constexpr const uint32 PORT_FANOUT = 1;
static constexpr const uint32 PORT_BW = 1;

#endif // PORTS_H
