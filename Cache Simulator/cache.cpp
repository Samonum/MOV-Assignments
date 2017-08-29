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
CacheLine Memory::READCL( address a , bool write)
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
// (for a passing grade (6)):
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
Cache::Cache( MemCac* mem, int cSize, int l )
{
	slotMask = ((cSize / SLOTSIZE / NWAYN) - 1) << 6;
	lot = new ParkingLot[cSize / SLOTSIZE / NWAYN];
	memory = mem;
	level = l;
	ResetStats();
}

// destructor
Cache::~Cache()
{
	delete lot;
}

// read a single byte from memory
byte Cache::READB(address a)
{
	return READCL(a & ADDRESSMASK).value[a & OFFSETMASK];
}

//Returns a 16-bit integer. Crashes when asked for values split over multiple cachelines
short Cache::READB16(address a)
{
	CacheLine line = READCL(a & ADDRESSMASK);
	return (line.value[a & OFFSETMASK] <<8) | line.value[(a&OFFSETMASK) + 1];
}

//Returns a 32-bit integer. Crashes when asked for values split over multiple cachelines
int Cache::READB32(address a)
{
	CacheLine line = READCL(a & ADDRESSMASK);
	return (line.value[a & OFFSETMASK] <<24) | (line.value[(a&OFFSETMASK) + 1] << 16) | 
		(line.value[(a&OFFSETMASK) + 2] << 8) | line.value[(a&OFFSETMASK) + 3];
}

// write a single byte to memory
void Cache::WRITEB(address a, byte value)
{
	CacheLine cl = READCL(a & ADDRESSMASK, true);
	cl.value[a&OFFSETMASK] = value;
	cl.tag |= DIRTY | VALID;

	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				slot[i] = cl;
				slot[i].tag |= DIRTY;
				return;
			}
	}
}

//Writes a 16-bit integer. Crashes when asked for values split over multiple cachelines
void Cache::WRITEB16(address a, short value)
{
	CacheLine cl = READCL(a & ADDRESSMASK, true);
	cl.value[a&OFFSETMASK] = value >> 8;
	cl.value[(a&OFFSETMASK) + 1] = value;
	cl.tag |= DIRTY | VALID;

	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				slot[i] = cl;
				slot[i].tag |= DIRTY;
				return;
			}
	}
}

//Writes a 32-bit integer. Crashes when asked for values split over multiple cachelines
void Cache::WRITEB32(address a, int value)
{
	CacheLine cl = READCL(a & ADDRESSMASK, true);
	cl.value[a&OFFSETMASK] = value >>24;
	cl.value[(a&OFFSETMASK) + 1] = value >> 16;
	cl.value[(a&OFFSETMASK) + 2] = value >> 8;
	cl.value[(a&OFFSETMASK) + 3] = value;
	cl.tag |= DIRTY | VALID;

	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				slot[i] = cl;
				slot[i].tag |= DIRTY;
				return;
			}
	}
}



// read a single byte from memory 
// isWrite parameter is used to display separate debug information for write and read operations
CacheLine Cache::READCL(address a, bool isWrite)
{
	_ASSERT((a & OFFSETMASK) == 0);
	if (!isWrite)
		read++;
	else
		write++;

	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;

	// -----------------------------------
	// Search for the address in the cache
	// -----------------------------------

	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if (a == (slot[i].tag & ADDRESSMASK))
			{
				if (!isWrite)
				{
					rHits++;
					rtotalHits++;
				}
				else
				{
					wHits++;
					wtotalHits++;
				}
#if EVICTION == 2
				//Set the marker bit for Bit-PLRU
				slot[i].tag |= LRUMARKER;
#elif EVICTION == 3 || EVICTION == 4
				//Update the tree for tree based PLRU implementations
				UpdateLRUTree(lot[(a&slotMask) >> 6], i);
#elif EVICTION == 5
				//Update all LRU counters for the full LRU
				slot[i].ltag = 0;
				slot[i].tag |= VALID;

				for (int j = 0; j < NWAYN; j++)
				{
					if (j == i)
						continue;
					slot[j].ltag++;
				}
#endif
				return slot[i];
			}
	}

	//If it's not found update the miss counter
	if (!isWrite)
	{
		rMisses++;
		rtotalMisses++;
	}
	else
	{
		wMisses++;
		wtotalMisses++;
	}

	//And search further
	return ReadMiss(a, isWrite);
}

#if EVICTION == 0
CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);
	// return the requested byte

	// -----------------------------------------------------
	// Try to find an invalid slot in the cache to overwrite
	// -----------------------------------------------------

	for (int i = 0; i < NWAYN; i++)
	{
		if (!slot[i].IsValid())
		{
			slot[i] = line;
			slot[i].tag = (a) | VALID;
			if (!write)
				rCacheAdd++;
			else
				wCacheAdd++;
			return line;
		}
	}

	// --------------------------------------
	// Evict a random slot to make room for new data
	// --------------------------------------

	int randomNumber = rand() % (NWAYN);

	if (slot[randomNumber].IsDirty())
		memory->WRITECL(slot[randomNumber].tag & ADDRESSMASK, slot[randomNumber]);

	slot[randomNumber] = line;
	slot[randomNumber].tag = a | VALID;
	if (!isWrite)
		rEvict++;
	else
		wEvict++;
	return line;
}


#elif EVICTION == 1
CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);

	// --------------------------------------
	// Evict the oldest slot
	// --------------------------------------

	if (slot[lot->evictionData].IsDirty())
		memory->WRITECL(slot[lot->evictionData].tag & ADDRESSMASK, slot[lot->evictionData]);

	slot[lot->evictionData] = line;
	slot[lot->evictionData].tag = a | VALID;
	//Update pointer to point to the next to be replaced element
	lot->evictionData = ++lot->evictionData & (NWAYN - 1);
	if (!isWrite)
		rEvict++;
	else
		wEvict++;
	return line;
}


#elif EVICTION == 2
CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);

	// -----------------------------------------------------
	// Try to find an invalid slot in the cache to overwrite
	// -----------------------------------------------------

	int target = -1;
	for (int i = 0; i < NWAYN; i++)
	{
		//If invalid, replace immediately
		if (!slot[i].IsValid())
		{
			slot[i] = line;
			slot[i].tag = a | VALID | LRUMARKER;
			if (!write)
				rCacheAdd++;
			else
				wCacheAdd++;
			return line;
		}
		//If the marler is not set, remember the current slot
		if (!(slot[i].tag & LRUMARKER))
			target = i;
	}

	//If all slots have their marker set to 1, set them to 0 and return the first slot
	if (target == -1)
	{
		for (int i = 0; i < NWAYN; i++)
			slot[i].tag &= ~LRUMARKER;
		target = 0;
	}
	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------

	if (slot[target].IsDirty())
		memory->WRITECL(slot[target].tag & ADDRESSMASK, slot[target]);

	slot[target] = line;
	slot[target].tag = a | VALID | LRUMARKER;
	if (!isWrite)
		rEvict++;
	else
		wEvict++;
	return line;
}


#elif EVICTION == 3

void Cache::UpdateLRUTree(ParkingLot &lot, int i)
{
	//Walk through the tree, analogues to binary search, updating all bits along the path
	int node = NWAYN / 2;
	for (int step = NWAYN / 2; step > 0;)
	{
		step /= 2;
		//Compare if the current node is bigger than the target node
		if (i < node)
		{
			//Set node to 0
			lot.evictionData &= ~(1 << node);
			node -= step;
		}
		else
		{
			//Set node to 1
			lot.evictionData |= 1 << node;
			node += step;
		}
	}

}

int Cache::TreeFindLRU(ParkingLot &lot)
{
	//Walk through the tree, analogues to binary search, updating all bits along the path
	int node = NWAYN / 2;
	for (int step = NWAYN / 2; step > 0; )
	{
		step /= 2;
		//Look if the current node is set to 1
		if ((lot.evictionData >> node) & 1)
		{
			//Set node to 0
			lot.evictionData &= ~(1 << node);
			node -= step;
		}
		else
		{
			//Set node to 1
			lot.evictionData |= 1 << node;
			node += step;
		}
	}
	//Return the leave indicated by the final node
	return node - (1 - ((lot.evictionData >> node) & 1));
}

CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);
	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------

	//Request the to be replaced address
	int target = TreeFindLRU(lot[(a&slotMask) >> 6]);
	if (slot[target].IsDirty())
		memory->WRITECL(slot[target].tag & ADDRESSMASK, slot[target]);

	slot[target] = line;
	slot[target].tag = a | VALID;
	if (!isWrite)
		rEvict++;
	else
		wEvict++;
	return line;
}

#elif EVICTION == 4

void Cache::UpdateLRUTree(ParkingLot &lot, int i)
{
	//Start at cetral node
	int node = NWAYN / 2;
	//Extract the history
	int hist = lot.evictionData >> 16;
	for (int step = NWAYN / 2; step > 0;)
	{
		//Update step size
		step /= 2;
		//Decide whether to move left or right
		if (i < node)
		{
			//If we're not at the bottom (non-leave) nodes
			if (step != 0)
			{
				//Set eviction data to the node history
				lot.evictionData ^= (-((hist >> node) & 1) ^ lot.evictionData) & (1 << node);
				//Update history
				hist &= ~(1 << node);
				node -= step;
			}
			else
				//Otherwise: Update node directly
				lot.evictionData &= ~(1 << node);
		}
		else
		{
			if (step != 0)
			{
				lot.evictionData ^= (-((hist >> node) & 1) ^ lot.evictionData) & (1 << node);
				hist |= 1 << node;
				node += step;
			}
			else
				lot.evictionData |= 1 << node;
		}
		//Return history
		lot.evictionData = (lot.evictionData & ((1 << 16) - 1)) + (hist << 16);
	}

}

int Cache::TreeFindLRU(ParkingLot &lot)
{
	//Extract history
	int hist = lot.evictionData >> 16;
	int node = NWAYN / 2;
	for (int step = NWAYN / 2; step > 0; )
	{
		step /= 2;
		if ((lot.evictionData >> node) & 1)
		{
			if (step != 0)
			{
				//Update history tree with current node
				lot.evictionData ^= (-((hist >> node) & 1) ^ lot.evictionData) & (1 << node);
				//Set the history node to 0
				hist &= ~(1 << node);
				node -= step;
			}
			else
				//Ignore history for the final node
				lot.evictionData &= ~(1 << node);
		}
		else
		{
			if (step != 0)
			{
				//Update history tree with current node
				lot.evictionData ^= (-((hist >> node) & 1) ^ lot.evictionData) & (1 << node);
				//Set the history node to 0
				hist |= 1 << node;
				node += step;
			}
			else
				//Ignore history for the final node
				lot.evictionData |= 1 << node;
		}
	}
	//Return hstory
	lot.evictionData = (lot.evictionData & ((1 << 16) - 1)) + (hist << 16);
	return node - (1 - ((lot.evictionData >> node) & 1));
}

CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	ParkingLot dbg = ParkingLot();
	dbg.evictionData = 0b00010000;
	int fewr = TreeFindLRU(dbg);

	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);
	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------
	int target = TreeFindLRU(lot[(a&slotMask) >> 6]);
	if (slot[target].IsDirty())
		memory->WRITECL(slot[target].tag & ADDRESSMASK, slot[target]);

	slot[target] = line;
	slot[target].tag = a | VALID;
	if (!isWrite)
		rEvict++;
	else
		wEvict++;
	return line;
}

#elif EVICTION == 5
CacheLine Cache::ReadMiss(address a, bool isWrite)
{
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;
	CacheLine line = memory->READCL(a, isWrite);

	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------
	
	for (int i = 0; i < NWAYN; i++)
	{
		if (!slot[i].IsValid())
		{
			slot[i] = line;
			slot[i].tag = (a & ADDRESSMASK) | VALID;
			slot[i].ltag = 0;

			for (int j = 0; j < NWAYN; j++)
			{
				if (j == i)
					continue;
				slot[j].ltag++;
			}
			rCacheAdd++;
			return line;
		}
	}

	// --------------------------------------
	// Evict a slot to make room for new data
	// --------------------------------------

	int maxTag = 0;
	int maxIndex = -1;

	//Find the cacheline that hasn't been used for the longest
	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].ltag > maxTag)
		{
			maxTag = slot[i].ltag;
			maxIndex = i;
		}
	}

	//Write it back if dirty
	if (slot[maxIndex].IsDirty())
		memory->WRITECL(slot[maxIndex].tag & ADDRESSMASK, slot[maxIndex]);

	slot[maxIndex] = line;
	slot[maxIndex].tag = (a & ADDRESSMASK) | VALID;
	slot[maxIndex].ltag = 0;

	//Increase the LRU counter for all but the current cacheline
	for (int i = 0; i < NWAYN; i++)
	{
		if (i == maxIndex)
			continue;
		slot[i].ltag++;
	}
	rEvict++;
	return line;
}
#endif

// write a single byte to memory
void Cache::WRITECL(address a, CacheLine& line)
{
	//Read the cacheline to make sure it's in L1
	CacheLine cl = READCL(a & ADDRESSMASK, true);
	CacheLine* slot = lot[(a&slotMask) >> 6].cacheLine;

	//Update the desired address
	for (int i = 0; i < NWAYN; i++)
	{
		if (slot[i].IsValid())
			if ((a & ADDRESSMASK) == (slot[i].tag & ADDRESSMASK))
			{
				slot[i] = line;
				slot[i].tag |= DIRTY;
				return;
			}
	}
}

void Cache::ResetStats()
{
	rHits = rMisses = rCacheAdd = rEvict = read = write = wEvict = wCacheAdd = wHits = wMisses = 0;
}

void Cache::ConsoleDebug()
{
	printf("L%i Total Read H/L:%i/%i Total Write H/L:%i/%i\n", level, rtotalHits, rtotalMisses, wtotalHits, wtotalMisses);
	printf("Read %i, hits:misses %i:%i, evictions / cacheAdds %i / %i\n", read, rHits, rMisses, rEvict, rCacheAdd);
	printf("Write %i, hits:misses %i:%i, evictions / cacheAdds %i / %i\n", write, wHits, wMisses, wEvict, wCacheAdd);
	printf("--------------------------------------------------------------------\n");
}