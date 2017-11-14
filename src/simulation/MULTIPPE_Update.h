#ifndef __MULTIPPE_Update_H__
#define __MULTIPPE_Update_H__

class Simulation;
class Renderer;
struct Particle;
class MULTIPPE_Update
{
public:
	MULTIPPE_Update() { }
	virtual ~MULTIPPE_Update() { }
	static Renderer * ren_;
	static int update(UPDATE_FUNC_ARGS);
	static int graphics(GRAPHICS_FUNC_ARGS);
	static void InsertText(Simulation *sim, int i, int x, int y, int ix, int iy);
	// static int AddCharacter(Simulation *sim, int x, int y, int c, int rgb);
	static void conductTo (Simulation* sim, int r, int x, int y, Particle *parts) // Inline or macro?
	{
		if (!parts[r>>8].life)
		{
			parts[r>>8].ctype = r&0xFF;
			sim->part_change_type(r>>8, x, y, PT_SPRK);
			parts[r>>8].life = 4;
		}
	}
	static void conductToSWCH (Simulation* sim, int r, int x, int y, Particle *parts) // Inline or macro?
	{
		if (parts[r>>8].life == 10)
		{
			parts[r>>8].ctype = r&0xFF;
			sim->part_change_type(r>>8, x, y, PT_SPRK);
			parts[r>>8].life = 4;
		}
	}
	// static bool SetDecoration(bool decorationState); // file used: src/gui/game/GameModel.cpp
	// static bool GetDecoration();
};
#endif
