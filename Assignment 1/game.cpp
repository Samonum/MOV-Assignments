#include "template.h"

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void Game::Init()
{
	// instantiate simulated memory and cache
	memory = new Memory( 1024 * 1024 ); // allocate 1MB
	cacheL3 = new Cache(memory, L3CACHESIZE, 3);
	cacheL2 = new Cache(cacheL3, L2CACHESIZE, 2);
	cacheL1 = new Cache(cacheL2, L1CACHESIZE, 1);
	// intialize fractal algorithm
	srand( 1000 );
	Set( 0, 0, IRand( 255 ) );
	Set( 512, 0, IRand( 255 ) );
	Set( 0, 512, IRand( 255 ) );
	Set( 512, 512, IRand( 255 ) );
	// put first subdivision task on stack
	taskPtr = 0;
	Push( 0, 0, 512, 512, 256 );
}

// -----------------------------------------------------------
// Helper functions for reading and writing data
// -----------------------------------------------------------
void Game::Set( int x, int y, byte value )
{
	address a = x + y * 513;
	cacheL1->WRITEB( a, value );
}
byte Game::Get( int x, int y )
{
	address a = x + y * 513;
	return cacheL1->READB( a );
}

// -----------------------------------------------------------
// Recursive subdivision of the height map
// -----------------------------------------------------------
void Game::Subdivide( int x1, int y1, int x2, int y2, int scale )
{
	// termination
	if ((x2 - x1) == 1) return;
	// calculate diamond vertex positions
	int cx = (x1 + x2) / 2, cy = (y1 + y2) / 2;
	// set vertices
	if (Get( cx, y1 ) == 0) Set( cx, y1, (Get( x1, y1 ) + Get( x2, y1 )) / 2 + IRand( scale ) - scale / 2 );
	if (Get( cx, y2 ) == 0) Set( cx, y2, (Get( x1, y2 ) + Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (Get( x1, cy ) == 0) Set( x1, cy, (Get( x1, y1 ) + Get( x1, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (Get( x2, cy ) == 0) Set( x2, cy, (Get( x2, y1 ) + Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (Get( cx, cy ) == 0) Set( cx, cy, (Get( x1, y1 ) + Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	// push new tasks
	Push( x1, y1, cx, cy, scale / 2 );
	Push( cx, y1, x2, cy, scale / 2 );
	Push( x1, cy, cx, y2, scale / 2 );
	Push( cx, cy, x2, y2, scale / 2 );
}

// -----------------------------------------------------------
// Main game tick function
// -----------------------------------------------------------
void Game::Tick( float dt )
{
	// execute 128 tasks per frame
	for( int i = 0; i < 128; i++ )
	{
		// execute one subdivision task
		if (taskPtr == 0) break;
		int x1 = task[--taskPtr].x1, x2 = task[taskPtr].x2;
		int y1 = task[taskPtr].y1, y2 = task[taskPtr].y2;
		Subdivide( x1, y1, x2, y2, task[taskPtr].scale );
	}
	// report on memory access cost (134M before your improvements :) )
	printf("--------------------------------------------------------------------\n");
	printf( "total memory access cost: %iM cycles\n", cacheL1->totalCost / 1000000 );
	printf("--------------------------------------------------------------------\n");
	cacheL1->ConsoleDebug();
	cacheL2->ConsoleDebug();
	cacheL3->ConsoleDebug();
	cacheL1->ResetStats();
	// visualize current state
	// artificial RAM access delay and cost counting are disabled here
	memory->artificialDelay = false, c = cacheL1->totalCost;
	for( int y = 0; y < 513; y++ ) for( int x = 0; x < 513; x++ ) 
		screen->Plot( x + 140, y + 60, GREY( Get( x, y ) ) );
	memory->artificialDelay = true, cacheL1->totalCost = c;
}

// -----------------------------------------------------------
// Clean up
// -----------------------------------------------------------
void Game::Shutdown()
{
	delete memory;
	delete cacheL1;
}