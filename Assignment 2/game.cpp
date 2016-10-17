#include "template.h"
#include <iostream>
#include <fstream>
#include <string>

#define DEV

// global data (source scope)
static Game* game;

// mountain peaks (push player away)
static float peakx[16] = { 248, 537, 695, 867, 887, 213, 376, 480, 683, 984, 364, 77,  85, 522, 414, 856 };
static float peaky[16] = { 199, 223, 83, 374,  694, 639, 469, 368, 545, 145,  63, 41, 392, 285, 447, 352 };
static float peakh[16] = { 200, 150, 160, 255, 200, 255, 200, 300, 120, 100,  80, 80,  80, 160, 160, 160 };

// player, bullet and smoke data
static int aliveP1 = MAXP1;
static int aliveP2 = MAXP2;
static Bullet bullet[MAXBULLET];
static GridCell tankGrid[GRIDY][GRIDX];
static GridCell teamGrid[2][GRIDY][GRIDX];
static float sinTable[720];
static float cosTable[720];
static float maxr;
static unsigned char mountainCircle[16][64];

// smoke particle effect tick function
void Smoke::Tick()
{
	unsigned int p = frame >> 3;

	if (frame < 64) 
		if (!(frame++ & 7))
		{
			puff[p].x = xpos;
			puff[p].y = ypos << 8; 
			puff[p].vy = -450;
			puff[p].life = 63;
		}

	for ( unsigned int i = 0; i < p; i++ ) 
		if ((frame < 64) || (i & 1))
		{
			puff[i].x++;
			puff[i].y += puff[i].vy;
			puff[i].vy += 3;

			int frame = (puff[i].life > 13) ? (9 - (puff[i].life - 14) / 5) : (puff[i].life / 2);

			game->m_Smoke->SetFrame(frame);
			game->m_Smoke->Draw(puff[i].x - 12,(puff[i].y >> 8) - 12, game->m_Surface);

			if (!--puff[i].life)
			{
				puff[i].x = xpos;
				puff[i].y = ypos << 8;
				puff[i].vy = -450;
				puff[i].life = 63;
			}
		}
}

// bullet Tick function
void Bullet::Tick()
{
	if (!(flags & Bullet::ACTIVE)) return;

	float2 prevpos = pos;
	pos += speed * 1.5f;
	prevpos -= pos - prevpos;
	game->m_Surface->AddLine( prevpos.x, prevpos.y, pos.x, pos.y, 0x555555 );

	if ((pos.x < 0) || (pos.x > (SCRWIDTH - 1)) || (pos.y < 0) || (pos.y > (SCRHEIGHT - 1))) 
		flags = 0; // off-screen

	unsigned int start = 0, end = MAXP1;

	if (flags & P1)
	{
		start = MAXP1;
		end = MAXP1 + MAXP2;
	}

	for ( unsigned int i = start; i < end; i++ ) // check all opponents
	{
		Tank* t = game->m_Tank[i];

		if (!((t->flags & Tank::ACTIVE) && (pos.x > (t->pos.x - 2)) && (pos.y > (t->pos.y - 2)) && (pos.x < (t->pos.x + 2)) && (pos.y < (t->pos.y + 2)))) 
			continue;

		// update counters
		if (t->flags & Tank::P1) 
			aliveP1--; 
		else 
			aliveP2--; 

		t->flags &= Tank::P1|Tank::P2;	// kill tank
		teamGrid[1 ^ (t->flags >> 2)][t->gridY()][t->gridX()].remove(t);
		flags = 0;						// destroy bullet
		break;
	}
}

// Tank::Fire - spawns a bullet
void Tank::Fire( unsigned int party, float2& pos, float2& dir )
{
	for ( unsigned int i = 0; i < MAXBULLET; i++ ) 
		if (!(bullet[i].flags & Bullet::ACTIVE))
		{
			bullet[i].flags |= Bullet::ACTIVE + party; // set owner, set active
			bullet[i].pos = pos;
			bullet[i].speed = dir;
			break;
		}
}

// Tank::Tick - update single tank
void Tank::Tick()
{
	if (!(flags & ACTIVE)) // dead tank
	{
		smoke.xpos = (int)pos.x;
		smoke.ypos = (int)pos.y;
		return smoke.Tick();
	}

	float2 force = normalize( target - pos );

	int grid_x = this->gridX();
	int grid_y = this->gridY();

	if (!(grid_y < GRIDY - 1 && grid_x < GRIDX - 1 && grid_y > 0 && grid_x > 0))
	{
		dir += force;
		dir = normalize(dir);
		pos += dir * maxspeed * 0.5f;
		if (active)
		{
			tankGrid[grid_y][grid_x].remove(this);
			teamGrid[1 ^ (flags >> 2)][grid_y][grid_x].remove(this);
			active = false;
		}
		return;
	}

	// evade mountain peaks
	for ( unsigned int i = 0; i < 16; i++ )
	{
		float2 d( pos.x - peakx[i], pos.y - peaky[i] );
		float sd = (d.x * d.x + d.y * d.y) * 0.2f;

		if (sd < 1500) 
		{
			force += d * 0.03f * (peakh[i] / sd);
			float r = sqrtf( sd );
			mountainCircle[i][(int)r]++;
		}
	}
		
	// evade other tanks
	for (int i = -1; i < 2; i++)
		for (int j = -1; j < 2; j++)
		{
			int curX = (grid_x + j) & GRIDXMASK;
			int curY = (grid_y + i) & GRIDYMASK;
			for (int k = 0; k < tankGrid[curY][curX].count; k++)
			{
				if (tankGrid[curY][curX].getTank(k) == this)
					continue;

				float2 d = pos - tankGrid[curY][curX].getTank(k)->pos;

				float squaredLength = d.x*d.x + d.y*d.y;

				if (squaredLength < 64)
					force += normalize(d) * 2.0f;
				else if (squaredLength < 256)
					force += normalize(d) * 0.4f;
			}
		}

	// evade user dragged line
	if ((flags & P1) && (game->m_LButton))
	{
		float x1 = (float)game->m_DStartX;
		float y1 = (float)game->m_DStartY;
		float x2 = (float)game->m_MouseX;
		float y2 = (float)game->m_MouseY;

		float2 N = normalize( float2( y2 - y1, x1 - x2 ) );
		float dist = dot( N, pos ) - dot( N, float2( x1, y1 ) );

		if (fabs(dist) < 10)
		{
			if (dist > 0) 
				force += N * 20;
			else 
				force -= N * 20;
		}
	}

	// update speed using accumulated force
	dir += force;
	dir = normalize(dir);
	pos += dir * maxspeed * 0.5f;
	int newGridX = gridX();
	int newGridY = gridY();
	if (!active)
	{
		active = true;
		tankGrid[newGridY][newGridX].add(this);
		teamGrid[1 ^ (flags >> 2)][newGridY][newGridX].add(this);
	}
	else if (newGridX != grid_x || newGridY != grid_y)
	{
		tankGrid[grid_y][grid_x].remove(this);
		tankGrid[newGridY][newGridX].add(this);
		teamGrid[1 ^ (flags >> 2)][grid_y][grid_x].remove(this);
		teamGrid[1 ^ (flags >> 2)][newGridY][newGridX].add(this);
	}

	// shoot, if reloading completed
	if (--reloading >= 0) 
		return;

	unsigned int start = 0;
	unsigned int end = MAXP1;

	if (flags & P1)
	{
		start = MAXP1;
		end = MAXP1 + MAXP2;
	}

	int hstart = -7 * (dir.x < -0.1);
	int hend = 7 * (dir.x > 0.1);
	int vstart = -7 * (dir.y < -0.1);
	int vend = 7 * (dir.y > 0.1);

	for (int i = vstart; i <= vend; i++)
		for (int j = hstart; j <= hend; j++)
		{
			int curX = (newGridX + j) & GRIDXMASK;
			int curY = (newGridY + i) & GRIDYMASK;
			int count = teamGrid[flags>>2][curY][curX].count;

			for (int k = 0; k<count; k++)
			{
				Tank* target = teamGrid[flags>>2][curY][curX].getTank(k);
				float2 d = target->pos - pos;
				float sqleng = d.x*d.x + d.y*d.y;

				if ((sqleng < 10000) && (dot(normalize(d), dir) > 0.99999f))
				{
					Fire(flags & (P1 | P2), pos, dir); // shoot
					reloading = 200; // and wait before next shot is ready
					return;
				}
			}
		}
}

// Game::Init - Load data, setup playfield
void Game::Init(bool loadState)
{
	m_Heights = new Surface("testdata/heightmap.png");
	m_Backdrop = new Surface(1024, 768);
	m_Grid = new Surface(1024, 768);

	Pixel* a1 = m_Grid->GetBuffer();
	Pixel* a2 = m_Backdrop->GetBuffer();
	Pixel* a3 = m_Heights->GetBuffer();

	for ( int y = 0; y < 768; y++ ) 
		for ( int idx = y * 1024, x = 0; x < 1024; x++, idx++ ) 
			a1[idx] = (((x & 31) == 0) | ((y & 31) == 0)) ? 0x6600 : 0;

	for ( int y = 0; y < 767; y++ ) 
		for ( int idx = y * 1024, x = 0; x < 1023; x++, idx++ ) 
		{
			float3 N = normalize(float3((float)(a3[idx + 1] & 255) - (a3[idx] & 255), 1.5f, (float)(a3[idx + 1024] & 255) - (a3[idx] & 255))), L(1, 4, 2.5f);

			float h = (float)(a3[x + y * 1024] & 255) * 0.0005f;
			float dx = x - 512.f;
			float dy = y - 384.f; 
			float d = sqrtf(dx * dx + dy * dy);
			float dt = dot(N, normalize(L));

			int u = max(0, min(1023, (int)(x - dx * h)));
			int v = max(0, min(767, (int)(y - dy * h)));
			int r = (int)Rand(255);

			a2[idx] = AddBlend(a1[u + v * 1024], ScaleColor(ScaleColor(0x33aa11, r) + ScaleColor(0xffff00, (255 - r)), (int)(max(0.0f, dt) * 80.0f) + 10));
		}

	for (int i = 0; i < 720; i++)
	{
		sinTable[i] = sinf((float)i * PI / 360.0f);
		cosTable[i] = cosf((float)i * PI / 360.0f);
	}
	m_P1Sprite = new Sprite( new Surface( "testdata/p1tank.tga" ), 1, Sprite::FLARE );
	m_P2Sprite = new Sprite( new Surface( "testdata/p2tank.tga" ), 1, Sprite::FLARE );
	m_PXSprite = new Sprite( new Surface( "testdata/deadtank.tga" ), 1, Sprite::BLACKFLARE );
	m_Smoke = new Sprite( new Surface( "testdata/smoke.tga" ), 10, Sprite::FLARE );

	if (!loadState)
	{
		m_Tank = new Tank*[MAXP1 + MAXP2];
		// create blue tanks
		for (unsigned int i = 0; i < MAXP1; i++)
		{
			Tank* t = m_Tank[i] = new Tank();
			t->pos = float2((float)((i % 40) * 20) - 500, (float)((i / 40) * 20)- 500);
			t->target = float2(SCRWIDTH, SCRHEIGHT); // initially move to bottom right corner
			t->dir = float2(0, 0);
			t->flags = Tank::ACTIVE | Tank::P1;
			t->maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
			t->id = i;

			int grid_x = t->gridX();
			int grid_y = t->gridY();

			if (!(grid_y < GRIDY - 1 && grid_x < GRIDX - 1 && grid_y > 0 && grid_x > 0))
			{
				t->active = false;
				continue;
			}

			tankGrid[grid_y][grid_x].add(t);
			teamGrid[1][grid_y][grid_x].add(t);
		}


		// create red tanks
		for (unsigned int i = 0; i < MAXP2; i++)
		{
			Tank* t = m_Tank[i + MAXP1] = new Tank();
			t->pos = float2((float)((i % 50) * 20) + 700, (float)((i / 50) * 20) - 500);
			//t->pos = float2((float)((i % 50) * 20 + 900), (float)((i / 50) * 20 + 600));
			t->target = float2(424, 336); // move to player base
			t->dir = float2(0, 0);
			t->flags = Tank::ACTIVE | Tank::P2;
			t->maxspeed = 0.3f;
			t->id = i + MAXP1;

			int grid_x = t->gridX();
			int grid_y = t->gridY();

			if (!(grid_y < GRIDY - 1 && grid_x < GRIDX - 1 && grid_y > 0 && grid_x > 0))
			{
				t->active = false;
				continue;
			}

			tankGrid[grid_y][grid_x].add(t);
			teamGrid[0][grid_y][grid_x].add(t);
		}
	}
	else
	{
		ifstream loadFile("save.state");

		if (!loadFile.is_open())
			return;

		string line;
		int teamFlag = 2;

		if (getline(loadFile, line))
		{
			delete m_Tank;
			m_Tank = new Tank*[stoi(line)];
		}

		float2 bluTarget;
		if (getline(loadFile, line))
		{
			string::size_type sz;
			bluTarget.x = std::stof(line, &sz);
			bluTarget.y = std::stof(line.substr(sz));
		}

		int i = 0;

		while (getline(loadFile, line))
		{
			if (line == "-")
				break;

			Tank* t = m_Tank[i] = new Tank();

			string::size_type sz;
			string::size_type fullSize = 0;

			t->flags = std::stoi(line, &sz);
			fullSize += sz;

			float x1 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			float y1 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			t->pos = float2(x1, y1);
			t->target = bluTarget;
			float x2 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			float y2 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			t->dir = float2(x2, y2);
			t->maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
			i++;

		}

		float2 redTarget;
		if (getline(loadFile, line))
		{
			string::size_type sz;
			redTarget.x = std::stof(line, &sz);
			redTarget.y = std::stof(line.substr(sz));
		}

		while (getline(loadFile, line))
		{
			Tank* t = m_Tank[i] = new Tank();

			string::size_type sz;
			string::size_type fullSize = 0;

			t->flags = std::stoi(line, &sz);
			fullSize += sz;

			float x1 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			float y1 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			t->pos = float2(x1, y1);
			t->target = redTarget;
			float x2 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			float y2 = std::stof(line.substr(fullSize), &sz);
			fullSize += sz;
			t->dir = float2(x2, y2);
			t->maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
			i++;
		}

		loadFile.close();
	}

	for (unsigned int i = 0; i < MAXBULLET; i++)
		bullet[i].flags = 0;

	aliveP1 = MAXP1;
	aliveP2 = MAXP2;

	game = this; // for global reference
	m_LButton = m_PrevButton = false;
}

// Game::DrawTanks - draw the tanks
void Game::DrawTanks()
{
	for ( unsigned int i = 0; i < (MAXP1 + MAXP2); i++ )
	{
		Tank* t = m_Tank[i];
		float x = t->pos.x, y = t->pos.y;
		float2 p1( x + 70 * t->dir.x + 22 * t->dir.y, y + 70 * t->dir.y - 22 * t->dir.x );
		float2 p2( x + 70 * t->dir.x - 22 * t->dir.y, y + 70 * t->dir.y + 22 * t->dir.x );

		if (!(m_Tank[i]->flags & Tank::ACTIVE)) 
			m_PXSprite->Draw( (int)x - 4, (int)y - 4, m_Surface ); // draw dead tank
		else if (t->flags & Tank::P1) // draw blue tank
		{
			m_P1Sprite->Draw( (int)x - 4, (int)y - 4, m_Surface );
			m_Surface->Line( x, y, x + 8 * t->dir.x, y + 8 * t->dir.y, 0x4444ff );
		}
		else // draw red tank
		{
			m_P2Sprite->Draw( (int)x - 4, (int)y - 4, m_Surface );
			m_Surface->Line( x, y, x + 8 * t->dir.x, y + 8 * t->dir.y, 0xff4444 );
		}

		if ((x >= 0) && (x < SCRWIDTH) && (y >= 0) && (y < SCRHEIGHT))
			m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH] = SubBlend( m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH], 0x030303 ); // tracks
	}
}

// Game::PlayerInput - handle player input
void Game::PlayerInput()
{
#ifndef DEV
	if (m_LButton)
	{
		// start line
		if (!m_PrevButton)
			m_DStartX = m_MouseX, m_DStartY = m_MouseY, m_DFrames = 0; 
		m_Surface->ThickLine( m_DStartX, m_DStartY, m_MouseX, m_MouseY, 0xffffff );
		m_DFrames++;
	}
	else
	{
		// new target location
		if ((m_PrevButton) && (m_DFrames < 15))
			for ( unsigned int i = 0; i < MAXP1; i++ ) m_Tank[i]->target = float2( (float)m_MouseX, (float)m_MouseY );

		m_Surface->Line( 0, (float)m_MouseY, SCRWIDTH - 1, (float)m_MouseY, 0xffffff );
		m_Surface->Line( (float)m_MouseX, 0, (float)m_MouseX, SCRHEIGHT - 1, 0xffffff );
	}
	m_PrevButton = m_LButton;	
#endif
}

void Tmpl8::Game::KeyDown(int a_Key)
{
}

void Tmpl8::Game::KeyUp(int a_Key)
{
	if (a_Key == 22)
		SaveState();
	else if (a_Key == 15)
		Init(true);
}

void Game::SaveState()
{
	ofstream saveFile("save.state", ios::trunc);

	saveFile << (MAXP1 + MAXP2) << "\n";

	if (MAXP1 > 0)
	{
		Tank* t = m_Tank[0];
		saveFile << t->target.x << DELIMITER << t->target.y << "\n";

		for (unsigned int i = 0; i < MAXP1; i++)
		{
			Tank* t = m_Tank[i];

			saveFile << t->flags << DELIMITER;
			saveFile << t->pos.x << DELIMITER << t->pos.y << DELIMITER;
			saveFile << t->dir.x << DELIMITER << t->dir.y;
			saveFile << "\n";
		}
	}

	saveFile << "-\n";

	if (MAXP2 <= 0)
	{
		saveFile.close();
		return;
	}

	Tank* t2 = m_Tank[MAXP1];
	saveFile << t2->target.x << DELIMITER << t2->target.y << "\n";

	for (unsigned int i = MAXP1; i < MAXP1 + MAXP2; i++)
	{
		Tank* t = m_Tank[i];

		saveFile << t->flags << DELIMITER;
		saveFile << t->pos.x << DELIMITER << t->pos.y << DELIMITER;
		saveFile << t->dir.x << DELIMITER << t->dir.y;
		saveFile << "\n";
	}

	saveFile.close();
}

// Game::Tick - main game loop
void Game::Tick( float a_DT )
{
	POINT p;
	GetCursorPos( &p );
	ScreenToClient( FindWindow( NULL, "Template" ), &p );
	m_LButton = (GetAsyncKeyState(VK_LBUTTON) != 0);
	m_MouseX = p.x;
	m_MouseY = p.y;
	m_Backdrop->CopyTo( m_Surface, 0, 0 );

	for ( unsigned int i = 0; i < (MAXP1 + MAXP2); i++ ) 
		m_Tank[i]->Tick();

	for ( unsigned int i = 0; i < MAXBULLET; i++ ) 
		bullet[i].Tick();

	for(int i =0; i < 16; i++)
		for(int r = 0; r < 64; r++)
			if(mountainCircle[i][r])
				for (int j = 0; j < 720; j++)
				{
					float x = peakx[i] + r * sinTable[j];
					float y = peaky[i] + r * cosTable[j];
					for(int k = 0; k < mountainCircle[i][r]; k++)
						game->m_Surface->AddPlot((int)x, (int)y, 0x000500);
				}
	memset(mountainCircle, false, 16 * 64);
	DrawTanks();
	PlayerInput();

	char buffer[128];

	if ((aliveP1 > 0) && (aliveP2 > 0))
	{
		sprintf( buffer, "blue army: %03i  red army: %03i", aliveP1, aliveP2 );
		return m_Surface->Print( buffer, 10, 10, 0xffff00 );
	}

	if (aliveP1 == 0) 
	{
		sprintf( buffer, "sad, you lose... red left: %i", aliveP2 );
		return m_Surface->Print( buffer, 200, 370, 0xffff00 );
	}

	sprintf( buffer, "nice, you win! blue left: %i", aliveP1 );

	m_Surface->Print( buffer, 200, 370, 0xffff00 );
}