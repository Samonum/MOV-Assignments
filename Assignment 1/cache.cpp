#include "template.h"

// ------------------------------------------------------------------
// SLOW RAM SIMULATOR
// Reads and writes full cachelines (64 bytes), like real RAM
// Has horrible performance, just like real RAM
// Should not be modified for the assignment.
// ------------------------------------------------------------------

// constructor
Memory::Memory( uint size )
{
	data = new CacheLine[size / SLOTSIZE];
	memset( data, 0, size );
	artificialDelay = true;
}

// destructor
Memory::~Memory()
{
	delete data;
}

// read a cacheline from memory
CacheLine Memory::READCL( address a )
{
	// verify that the requested address is the start of a cacheline in memory
	_ASSERT( (a & OFFSETMASK) == 0 );
	// simulate the slowness of RAM
	if (artificialDelay) delay();
	// return the requested data
	return data[a / SLOTSIZE];
}

// write a cacheline to memory
void Memory::WRITECL( address a, CacheLine& line )
{
	// verify that the requested address is the start of a cacheline in memory
	_ASSERT( (a & OFFSETMASK) == 0 );
	// simulate the slowness of RAM
	if (artificialDelay) delay();
	// write the supplied data to memory
	data[a / SLOTSIZE] = line;
}

// ------------------------------------------------------------------
// CACHE SIMULATOR
// Currently passes all requests directly to simulated RAM.
// TODO (for a passing grade (6)):
// 1. Build a fully associative cache to speed up requests
// 2. Build a direct mapped cache to improve on the fully associative
//    cache
// 3. Build a N-way set associative cache to improve on the direct
//    mapped cache
// Optional (1 extra point per item, up to a 10):
// 4. Experiment with various eviction policies
// 5. Build a cache hierarchy
// 6. Implement functions for reading/writing 16 and 32-bit values
// 7. Provide detailed statistics on cache performance
// ------------------------------------------------------------------

// constructor
Cache::Cache( MemCac* mem, int cSize )
{
	cacheSize = cSize;
	lot = new ParkingLot[cSize / SLOTSIZE / NWAYN];
	memory = mem;

	totalCost = 0;
	ResetStats();
}

// destructor
Cache::~Cache()
{
	delete lot;
}

// read a single byte from memory
// TODO: minimize calls to memory->READ using caching
byte Cache::READB(address a)
{
	read++;
	CacheLine* slot = lot[(a&SLOTMASK) >> 6].cacheLine;

	// -----------------------------------
	// Search for the address in the cache
	// -----------------------------------

	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				rHits++;
				totalCost += L1ACCESSCOST;
				return slot[i].value[a & OFFSETMASK];
			}
	}

	rMisses++;
	// update memory access cost
	totalCost += RAMACCESSCOST;	// TODO: replace by L1ACCESSCOST for a hit
	// request a full line from memory
	CacheLine line = memory->READCL(a & ADDRESSMASK);
	// return the requested byte
	byte returnValue = line.value[a & OFFSETMASK];
	
	// -----------------------------------------------------
	// Try to find an invalid slot in the cache to overwrite
	// -----------------------------------------------------

	for (int i = 0; i < NWAYN; i++)
	{
		if (!slot[i].IsValid())
		{
			slot[i] = line;
			slot[i].tag = (a & ADDRESSMASK) | VALID;
			rCacheAdd++;
			return returnValue;
		}
	}
	
	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------

	int randomNumber = rand() % (NWAYN);

	if (slot[randomNumber].IsDirty())
		memory->WRITECL(slot[randomNumber].tag & ADDRESSMASK, slot[randomNumber]);

	slot[randomNumber] = line;
	slot[randomNumber].tag = (a & ADDRESSMASK) | VALID;
	rEvict++;
	return returnValue;
}

// write a single byte to memory
// TODO: minimize calls to memory->WRITE using caching
void Cache::WRITEB( address a, byte value )
{
	write++;
	CacheLine* slot = lot[(a&SLOTMASK) >> 6].cacheLine;

	// -----------------------------------
	// Search for the address in the cache
	// -----------------------------------

	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				slot[i].value[a & OFFSETMASK] = value;
				slot[i].tag |= DIRTY;
				wHits++;
				totalCost += L1ACCESSCOST;
				return;
			}
	}

	// -----------------------------------------------------
	// Try to find an invalid slot in the cache to overwrite
	// -----------------------------------------------------

	wMisses++;
	totalCost += RAMACCESSCOST;

	CacheLine line = memory->READCL(a & ADDRESSMASK);
	for (int i = 0; i < NWAYN; i++)
	{
		if (!slot[i].IsValid())
		{
			// change the byte at the correct offset
			line.value[a & OFFSETMASK] = value;
			line.tag = (a & ADDRESSMASK) | VALID | DIRTY;
			slot[i] = line;
			wCacheAdd++;
			return;
		}
	}

	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------

	int randomNumber = rand() % (NWAYN);

	if (slot[randomNumber].IsDirty())
		memory->WRITECL(slot[randomNumber].tag & ADDRESSMASK, slot[randomNumber]);

	// change the byte at the correct offset
	line.value[a & OFFSETMASK] = value;
	line.tag = (a & ADDRESSMASK) | VALID | DIRTY;
	slot[randomNumber] = line;
	wEvict++;
	return;
}

void Cache::ResetStats()
{
	rHits = rMisses = rCacheAdd = rEvict = read = write = wEvict = wCacheAdd = wHits = wMisses = 0;
}