#pragma once

#define L1CACHESIZE		8192					// total L1$ size, in bytes
#define L2CACHESIZE		16384					// total L2$ size, in bytes
#define L3CACHESIZE		65536					// total L3$ size, in bytes
#define SLOTSIZE		64						// cache slot size, in bytes
#define ADDRESSMASK		(0x1000000 - SLOTSIZE)	// used for masking out lowest log2(SLOTSIZE) bits
#define OFFSETMASK		(SLOTSIZE - 1)			// used for masking out bits above log2(SLOTSIZE)
#define RAMACCESSCOST	110
#define L1ACCESSCOST	4
#define L2ACCESSCOST	16
#define L3ACCESSCOST	48
#define VALID           0b1
#define DIRTY           0b10

typedef unsigned int address;

struct CacheLine
{
	uint tag = 0;
	byte value[SLOTSIZE];
	bool IsValid();
	bool IsDirty();
};

inline bool CacheLine::IsValid()
{
	return tag & VALID;
}

inline bool CacheLine::IsDirty()
{
	return tag & DIRTY;
}


class Memory
{
public:
	// ctor/dtor
	Memory( uint size );
	~Memory();
	// methods
	CacheLine READ( address a );
	void WRITE( address a, CacheLine& line );
	// data members
	CacheLine* data;
	int* tag;
	bool artificialDelay;
};

class Cache
{
public:
	// ctor/dtor
	Cache( Memory* mem );
	~Cache();
	// methods
	byte READ( address a );
	void WRITE( address a, byte );
	void ResetStats();
	// TODO: READ/WRITE functions for (aligned) 16 and 32-bit values
	// data
	CacheLine* slot;
	Memory* memory;
	int hits, misses, totalCost, cacheAdd;
};