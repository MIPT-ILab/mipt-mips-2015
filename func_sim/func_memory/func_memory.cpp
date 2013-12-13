/**
 * func_memory.cpp - the module implementing the concept of
 * programer-visible memory space accesing via memory address.
 * @author Alexander Titov <alexander.igorevich.titov@gmail.com>
 * Copyright 2012 uArchSim iLab project
 */

// Generic C

// Generic C++

// uArchSim modules
#include <func_memory.h>

//method to get mask ( 1 in pleces from first to last, including ends)
//from 1 to 64
uint64 getMask(int first, int last);//capter, implementation below...


FuncMemory::FuncMemory( const char* executable_file_name,
                        uint64 addr_size,
                        uint64 page_bits,
                        uint64 offset_bits)
{
    //memory initialisation
    assert( addr_size > page_num_size+ offset_size ); //-------
    assert( addr_size <= 64 );                        // Critical
    assert( page_num_size > 0 );                      // tests
    assert( offset_size > 0 );                        //-------
    //Constants definition
    this->addr_size = addr_size;
    this->sets_num_size = addr_size- page_num_size - offset_size;
    this->page_num_size = page_num_size;
    this->offset_size = offset_size;
    this->sets_num_mask = getMask( this->addr_size -1,
                                   page_num_size+
                                   offset_size);
    this->page_num_mask = getMask( page_num_size+
                                   offset_size -1,
                                   offset_size);
    this->offset_mask = getMask( offset_size-1,
                                 0);
    //allocating memory for main set
    this->sets=( uint8**) malloc( sizeof( uint64) *
                                ( 1 << this->sets_num_size) );
    //Main set initialisation. It is important!
    for( int i = 0; i < ( 1 << this->sets_num_size) ; i++ )
    {
        this->sets[i] = NULL;
    }   
    
    //data initialisation
    vector<ElfSection> sections;
    ElfSection::getAllElfSections( executable_file_name, sections);
    vector<ElfSection>::iterator section;
    uint64 i;
    for ( section = sections.begin(); section != sections.end(); section++)
    {
        uint64 section_start_addr = section->start_addr;
        uint64 section_size = section->size;
        for( i =0 ; i< section_size; i++)
        {
            this->write( section->content[i] , i + section_start_addr, 4 );
        }
        if( strcmp( section->name, ".text") == 0)
            this->setStartPC( section->start_addr );
    }

}

//This funktion writes num_of_butes of bytes to addr
uint64 FuncMemory::read( uint64 addr, unsigned short num_of_bytes) //const
{
    assert( num_of_bytes > 0);
    uint64 addr_ = addr;
    uint8 temp;
    uint64 value = 0;
    uint8 *Ptr =(uint8*) &value;
    for ( int i = 0 ; i < num_of_bytes ; i++ )
    {
        *Ptr= getByteFromAddr( addr_);
        Ptr++;
        addr_++;
    }
    return value;
}

//It take byteADDRES and returns byte's value
uint8 FuncMemory::getByteFromAddr( uint64 addr )
{
    uint64 sets_num = getSetsNum( addr );
    uint64 page_num = getPageNum( addr );
    uint64 offset = getOffset( addr );
    uint8* pages = this->sets[sets_num];
    
    assert( ( 1 << (this->addr_size) ) <= addr );
    assert( pages != NULL );

    return getByteFromPages( ( uint8**) pages, page_num, offset);
}

// Like previous. It takes PagePtr, PageOffset and Offset of byte and
// returns it's byte 
uint8 FuncMemory::getByteFromPages( uint8** pages, uint64 page_num,
                            uint64 offset)
{
    assert( pages[page_num] != NULL );
    return pages[page_num][offset];
}

//This funktion read num_of_butes of bytes from addr
void FuncMemory::write( uint64 value, uint64 addr, unsigned short num_of_bytes)
{
    assert( num_of_bytes > 0);
    uint64 addr_ = addr;
    uint64 value_ = value;
    uint8 *Ptr =(uint8*) &value_;
    for ( int i = 0  ; i < num_of_bytes ; i++ )
    {
        setByteToAddr( addr_, Ptr[i]);
        addr_++;
    }
}

//It take byteADDRES and byte and write byte to this addres.
void FuncMemory::setByteToAddr( uint64 addr, uint8 byte)
{
    uint64 sets_num = getSetsNum( addr );
    uint64 page_num = getPageNum( addr );
    uint64 offset = getOffset( addr );
    uint8* pages = this->sets[sets_num];
    if ( pages  == NULL )
    {
        pages = allocateSet();
        this->sets[sets_num]= pages;
    }
    setByteToPages( (uint8**) pages, page_num , offset, byte );
}

//Like previous. It take PagePtr.. and byte and write byte to this addres.
void FuncMemory::setByteToPages( uint8** pages, uint64 page_num,
                          uint64 offset, uint8 byte)
{
    if ( pages[page_num] == NULL )
        pages[page_num] = allocatePage();
    pages[page_num][offset] =  byte;
}

//It is clear
uint64 FuncMemory::setStartPC( uint64 startPC )
{
    return this->startPC_var = startPC;
}

//It is clear
uint64 FuncMemory::getStartPC()
{
    return this->startPC_var;
}

//Returns pointer to free memory for one page
uint8* FuncMemory::allocatePage()
{
    uint8* page = (uint8*) malloc(sizeof(uint8) << this->offset_size );
    assert( page!=NULL );
    return page;
}

//Returns pointer to free memory for one set of pages
uint8* FuncMemory::allocateSet()
{
    uint8* pages = (uint8*) malloc(sizeof(uint8*) << this->page_num_size );
    assert( pages!=NULL );
    for( uint64 i = 0; i < (1<< this->page_num_size ); i++)
    {
        ((uint8**) pages)[i] = NULL;
    }
    return pages;
}


//It takes addres and returns Sets offset
uint64 VirtualMemory::getSetsNum( uint64 addr)
{
    return (addr & this->sets_num_mask) >> 
            (this->page_num_size + this->offset_size);
}

//It takes addres and returns Pages offset
uint64 VirtualMemory::getPageNum( uint64 addr)
{
    return (addr & this->page_num_mask) >> 
            (this->offset_size);
}

//It takes addres and returns Offset
uint64 VirtualMemory::getOffset( uint64 addr)
{
    return (addr & this->offset_mask);
}


FuncMemory::~FuncMemory()
{
//Recursive freing of allocated memory;
    for ( uint64 i = 0 ; i < ( 1 << this->sets_num_size) ; i++)
    {
        if ( this->sets[i] == NULL )
            continue;
        else
        {
            for( uint64 j = 0; j < ( 1 << this->page_num_size); j++)
            {
                if ( (  (( uint8** )this->sets[i])[j] ) == NULL )
                    continue;
                else
                   free (  (( uint8** )this->sets[i])[j]);
            }
            free(this->sets[i]);
        }
    }
}

uint64 FuncMemory::startPC() //const
{
   return this->getStartPC();
}

string FuncMemory::dump( string indent) //const
{
    // put your code here
    return string("ERROR: You need to implement FuncMemory!");
}


//returns mask, like this 000000011110000
uint64 getMask(int first, int last)
{
    uint64 temp = (uint64) ( (int64) -1 );
    temp = ( temp >> last ) << last;
    temp = ( temp << (63 - first) ) >> (63 - first);
    return temp;
}
