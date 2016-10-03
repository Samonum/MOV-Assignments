#include "template.h"


unsigned char m[513 * 513];
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
	m[a] = value;
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
	printf( "total memory access cost: %iK cycles\n", ((cacheL1->rtotalHits + cacheL1->wtotalHits) * L1ACCESSCOST + (cacheL2->rtotalHits + cacheL2->wtotalHits) * L2ACCESSCOST + (cacheL3->rtotalHits + cacheL3->wtotalHits) * L3ACCESSCOST + (cacheL3->rtotalMisses + cacheL3->wtotalMisses) * RAMACCESSCOST)/ 1000 );
	printf("--------------------------------------------------------------------\n");
	cacheL1->ConsoleDebug();
	cacheL2->ConsoleDebug();
	cacheL3->ConsoleDebug();
	DrawGraph();
	cacheL1->ResetStats();
	cacheL2->ResetStats();
	cacheL3->ResetStats();
	// visualize current state
	// artificial RAM access delay and cost counting are disabled here

	
	for (int y = 0; y < 513; y++) for (int x = 0; x < 513; x++)
		screen->Plot(x + 140, y + 60, GREY(m[x + y * 513]));
}

void Game::DrawGraph()
{
	int l1 = (cacheL1->rHits + cacheL1->wHits) * L1ACCESSCOST;
	int l2 = (cacheL2->rHits + cacheL2->wHits) * L2ACCESSCOST;
	int l3 = (cacheL3->rHits + cacheL3->wHits) * L3ACCESSCOST;
	int mem = (cacheL3->rMisses + cacheL3->wMisses) * RAMACCESSCOST;
	int total = l1 + l2 + l3 + mem;
	if (total == 0)
		return;
	//float l1p = (float)l1 / (float)total;
	float l2p = (float)l2 / (float)total;
	float l3p = (float)l3 / (float)total;
	float memp = (float)mem / (float)total;
	int graphHeight = SCRHEIGHT - 580;
	int y1 = 580 + graphHeight * memp;
	int y2 = y1 + graphHeight * l3p;
	int y3 = y2 + graphHeight * l2p;

	performanceGraph[graphPointer][0] = y1;
	performanceGraph[graphPointer][1] = y2;
	performanceGraph[graphPointer][2] = y3;

	float percent = (float)total / 10000.0f;
	totalWorkGraph[graphPointer] = SCRHEIGHT - graphHeight * percent;

	for (int y = 580; y < SCRHEIGHT; y++)
		for (int x = 0; x < graphPointer; x++)
			screen->Plot(x, y, DARKNESS);

	for (int x = 0; x < graphPointer; x++)
	{
		for (int y = 580; y < performanceGraph[x][0]; y++)
			screen->Plot(x * 4, y, RAMCOLOR);
		for (int y = performanceGraph[x][0]; y < performanceGraph[x][1]; y++)
			screen->Plot(x * 4, y, L3CACHECOLOR);
		for (int y = performanceGraph[x][1]; y < performanceGraph[x][2]; y++)
			screen->Plot(x * 4, y, L2CACHECOLOR);
		for (int y = performanceGraph[x][2]; y < SCRHEIGHT; y++)
			screen->Plot(x * 4, y, L1CACHECOLOR);
		for (int y = (totalWorkGraph[x] - 2); y < (totalWorkGraph[x] + 2); y++)
		{
			if (totalWorkGraph[x] < performanceGraph[x][2])
				screen->Plot(x * 4, y, HIBLU);
			else
				screen->Plot(x * 4, y, LOBLU);
		}
			
	}
	for (int y = 580; y < SCRHEIGHT; y++)
		screen->Plot(graphPointer * 4, y, DARKNESS);

	if (graphPointer < SCRWIDTH / 4 - 1)
		graphPointer++;
	else
		graphPointer = 0;
}

// -----------------------------------------------------------
// Clean up
// -----------------------------------------------------------
void Game::Shutdown()
{
	delete memory;
	delete cacheL1;
}
