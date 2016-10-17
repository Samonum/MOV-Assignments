#ifndef I_GAME_H
#define I_GAME_H

#define	SCRWIDTH	1024
#define SCRHEIGHT	768
#define GRIDSIZE	16
#define GRIDX		1024/GRIDSIZE
#define GRIDY		8192/GRIDSIZE
#define GRIDXMASK	GRIDX - 1
#define GRIDYMASK	GRIDY - 1

namespace Tmpl8 {

#define MAXP1		5000				// increase to test your optimized code
#define MAXP2		(4 * MAXP1)	// because the player is smarter than the AI
#define MAXBULLET	200
#define DELIMITER   ' '

class Smoke
{
public:
	struct Puff { int x, y, vy, life; };
	Smoke() : active( false ), frame( 0 ) {};
	void Tick();
	Puff puff[8];
	bool active;
	int frame, xpos, ypos;
};

class Tank
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Tank() : pos( float2( 0, 0 ) ), dir( float2( 0, 0 ) ), target( float2( 0, 0 ) ), reloading( 0 ) {};
	~Tank();
	void Fire( unsigned int party, float2& pos, float2& dir );
	void Tick();
	float2 pos, dir, target;
	float maxspeed;
	int flags, reloading;
	unsigned short id;
	Smoke smoke;
	inline int gridX() { return ((int)pos.x >> 5) & (GRIDX - 1); };
	inline int gridY() { return ((int)pos.y >> 5) & (GRIDY - 1); };
};

class Bullet
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Bullet() : flags( 0 ) {};
	void Tick();
	float2 pos, speed;
	int flags;
};

class Surface;
class Surface8;
class Sprite;
class Game
{
public:
	void SetTarget( Surface* a_Surface ) { m_Surface = a_Surface; }
	void MouseMove( int x, int y ) { m_MouseX = x; m_MouseY = y; }
	void MouseButton( bool b ) { m_LButton = b; }
	void Init(bool loadState);
	void UpdateTanks();
	void UpdateBullets();
	void DrawTanks();
	void PlayerInput();
	void KeyDown(int a_Key);
	void KeyUp(int a_Key);
	void SaveState();
	void LoadState();
	void Tick( float a_DT );
	Surface* m_Surface, *m_Backdrop, *m_Heights, *m_Grid;
	Sprite* m_P1Sprite, *m_P2Sprite, *m_PXSprite, *m_Smoke;
	int m_ActiveP1, m_ActiveP2;
	int m_MouseX, m_MouseY, m_DStartX, m_DStartY, m_DFrames;
	bool m_LButton, m_PrevButton;
	Tank** m_Tank;
};

__declspec(align(64)) struct GridCell
{
	unsigned int count = 0;
	unsigned int index[63];
	inline void add(Tank* newindex) { index[count] = reinterpret_cast<int>(newindex); count++; };
	inline void remove(Tank* oldindex)
	{
		int i = 0;
		while (index[i] != reinterpret_cast<int>(oldindex))
			i++;
		index[i] = index[--count];
	};
	inline Tank* getTank(int i)
	{
		return reinterpret_cast<Tank*>(index[i]);
	}
};

}; // namespace Templ8

#endif