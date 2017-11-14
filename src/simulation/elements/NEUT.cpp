#include "simulation/Elements.h"
//#TPT-Directive ElementClass Element_NEUT PT_NEUT 18
Element_NEUT::Element_NEUT()
{
	Identifier = "DEFAULT_PT_NEUT";
	Name = "NEUT";
	Colour = PIXPACK(0x20E0FF);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 1.00f;
	Collision = -0.99f;
	Gravity = 0.0f;
	Diffusion = 0.01f;
	HotAir = 0.002f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = -1;

	Temperature = R_TEMP+4.0f	+273.15f;
	HeatConduct = 60;
	Description = "Neutrons. Interact with matter in odd ways.";

	Properties = TYPE_ENERGY|PROP_LIFE_DEC|PROP_LIFE_KILL_DEC;
	Properties2 |= PROP_ENERGY_PART | PROP_NEUTRONS_LIKE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &Element_NEUT::update;
	Graphics = &Element_NEUT::graphics;
}

//#TPT-Directive ElementHeader Element_NEUT static int update(UPDATE_FUNC_ARGS)
int Element_NEUT::update(UPDATE_FUNC_ARGS)
{
	int r, rx, ry, target_r = -1;
	int iX = 0, iY = 0;
	int pressureFactor = 3 + (int)sim->pv[y/CELL][x/CELL];
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK)
			{
				r = pmap[y+ry][x+rx];
				switch (r&0xFF)
				{
				case PT_WATR:
					if (3>(rand()%20))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_DSTW);
				case PT_ICEI:
				case PT_SNOW:
					parts[i].vx *= 0.995;
					parts[i].vy *= 0.995;
					break;
				case PT_PLUT:
					if (pressureFactor>(rand()%1000))
					{
						if (!(rand()%3))
						{
							sim->create_part(r>>8, x+rx, y+ry, rand()%3 ? PT_LAVA : PT_URAN);
							parts[r>>8].temp = MAX_TEMP;
							if (parts[r>>8].type==PT_LAVA) {
								parts[r>>8].tmp = 100;
								parts[r>>8].ctype = PT_PLUT;
							}
						}
						else
						{
							sim->create_part(r>>8, x+rx, y+ry, PT_NEUT);
							parts[r>>8].vx = 0.25f*parts[r>>8].vx + parts[i].vx;
							parts[r>>8].vy = 0.25f*parts[r>>8].vy + parts[i].vy;
						}
						sim->pv[y/CELL][x/CELL] += 10.0f * CFDS; //Used to be 2, some people said nukes weren't powerful enough
						Element_FIRE::update(UPDATE_FUNC_SUBCALL_ARGS);
					}
					break;
#ifdef SDEUT
				case PT_DEUT:
					if ((pressureFactor+1+(parts[r>>8].life/100))>(rand()%1000))
					{
						DeutExplosion(sim, parts[r>>8].life, x+rx, y+ry, restrict_flt(parts[r>>8].temp + parts[r>>8].life*500.0f, MIN_TEMP, MAX_TEMP), PT_NEUT);
						sim->kill_part(r>>8);
					}
					break;
#else
				case PT_DEUT:
					if ((pressureFactor+1)>(rand()%1000))
					{
						create_part(r>>8, x+rx, y+ry, PT_NEUT);
						parts[r>>8].vx = 0.25f*parts[r>>8].vx + parts[i].vx;
						parts[r>>8].vy = 0.25f*parts[r>>8].vy + parts[i].vy;
						parts[r>>8].life --;
						parts[r>>8].temp = restrict_flt(parts[r>>8].temp + parts[r>>8].life*17.0f, MIN_TEMP, MAX_TEMP);
						pv[y/CELL][x/CELL] += 6.0f * CFDS;

					}
					break;
#endif
				case PT_GUNP:
					if (3>(rand()%200))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_DUST);
					break;
				case PT_DYST:
					if (3>(rand()%200))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_YEST);
					break;
				case PT_YEST:
					sim->part_change_type(r>>8,x+rx,y+ry,PT_DYST);
					break;
				case PT_PLEX:
					if (3>(rand()%200))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_GOO);
					break;
				case PT_NITR:
					if (3>(rand()%200))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_DESL);
					break;
				case PT_PLNT:
					if (!(rand()%20))
						sim->create_part(r>>8, x+rx, y+ry, PT_WOOD);
					break;
				case PT_DESL:
				case PT_OIL:
					if (3>(rand()%200))
						sim->part_change_type(r>>8,x+rx,y+ry,PT_GAS);
					break;
				case PT_COAL:
					if (!(rand()%20))
						sim->create_part(r>>8, x+rx, y+ry, PT_WOOD);
					break;
				case PT_BCOL:
					if (!(rand()%20))
						sim->create_part(r>>8, x+rx, y+ry, PT_SAWD);
					break;
				case PT_DUST:
					if (!(rand()%20))
						sim->part_change_type(r>>8, x+rx, y+ry, PT_FWRK);
					break;
				case PT_FWRK:
					if (!(rand()%20))
						parts[r>>8].ctype = PT_DUST;
					break;
				case PT_ACID:
					if (!(rand()%20))
						sim->create_part(r>>8, x+rx, y+ry, PT_ISOZ);
					break;
				case PT_TTAN:
					if (!(rand()%20))
					{
						sim->kill_part(i);
						return 1;
					}
					break;
				case PT_EXOT:
					if (!(rand()%20))
						parts[r>>8].life = 1500;
					break;
				case PT_RFRG:
					if (rand()%2)
						sim->create_part(r>>8, x+rx, y+ry, PT_GAS);
					else
						sim->create_part(r>>8, x+rx, y+ry, PT_CAUS);
					break;
				case PT_POLC:
					if (parts[r>>8].tmp && !(rand()%80))
						parts[r>>8].tmp--;
					break;
				case ELEM_MULTIPP:
					{
					int rr, j, nr;
					if (parts[r>>8].life == 22)
					{
						switch (parts[r>>8].tmp >> 3)
						{
						case 1:
							parts[i].vx = 0, parts[i].vy = 0;
							break;
						case 2:
							if (parts[r>>8].temp > 8000)
							{
								int temp = (int)(parts[r>>8].temp - 8272.65f);
								if (!(rx || ry) && temp > (int)(rand() * 1000.0f / (RAND_MAX+1.0f)))
								{
									sim->kill_part(i);
									return 1;
								}
							}
							else if (!(rand()%25))
							{
								rr = sim->create_part(-1, x, y, PT_ELEC);
								if (rr >= 0)
								{
									parts[i].tmp2 = 1;
									sim->part_change_type(i, x, y, PT_PROT);
								}
							}
							break;
						case 3:
							if (!(parts[r>>8].tmp2 || rand()%500))
							{
								int np = sim->create_part(-1, x, y, PT_NEUT);
								if (np < 0) parts[np].temp = parts[i].temp;
								parts[r>>8].tmp2 = 500;
							}
							if (!(rx || ry))
							{
								sim->ineutcount++;
							}
							break;
						case 4:
							parts[i].vx *= 0.995;
							parts[i].vy *= 0.995;
							break;
						}
					}
					else if (parts[r>>8].life <= 8 && !(rx || ry))
					{
						if (parts[r>>8].life == 5 && !parts[r>>8].tmp)
							target_r = r>>8;
						if (parts[r>>8].life != 8)
							continue;
						parts[i].vx = 0, parts[i].vy = 0;
						for (j = 0; j < 5; j++)
						{
							iX = rand() % (ISTP * 2 + 1) - ISTP;
							iY = rand() % (ISTP * 2 + 1) - ISTP;
							rr = pmap[y+iY][x+iX];
							if ((rr&0xFF) == ELEM_MULTIPP && parts[rr>>8].life == 8)
								break;
						}
						if (j == 5)
							iY = 0, iX = 0;
					}
					else if (parts[r>>8].life == 16 && parts[r>>8].ctype == 25)
					{
						int tmp2 = parts[r>>8].tmp2;
						int multiplier = (tmp2 >> 4) + 1;
						tmp2 &= 0x0F;
						if (Element_MULTIPP::Arrow_keys & 0x10 && tmp2 >= 1 && tmp2 <= 8)
						{
							iX += multiplier*sim->portal_rx[tmp2-1];
							iY += multiplier*sim->portal_ry[tmp2-1];
						}
					}
					}
					break;
				default:
					break;
				}
			}
	if (target_r >= 0)
	{
		ChangeDirection(sim, i, x, y, &parts[i], &parts[target_r]);
	}
	else
	{
		parts[i].vx += (float)iX;
		parts[i].vy += (float)iY;
	}
	return 0;
}


//#TPT-Directive ElementHeader Element_NEUT static void ChangeDirection(Simulation* sim, int i, int x, int y, Particle* neut, Particle* under)
void Element_NEUT::ChangeDirection(Simulation* sim, int i, int x, int y, Particle* neut, Particle* under)
{
	if (under->tmp2 == 18)
	{
		neut->ctype = 0x100;
		neut->tmp2 = 0x3FFFFFFF;
		sim->part_change_type(i, x, y, PT_E186);
	}
	else if (under->tmp2 == 25)
	{
		int rr = under->ctype;
		float angle = rand() / (float)(RAND_MAX) - 0.5f;
		float radius = (float)(rr >> 4) / 16.0f;
		if (rr & 8) angle *= 0.5f;
		angle += (float)(rr & 7) / 4.0f;
		neut->vx = sinf(angle * M_PI) * radius;
		neut->vy = cosf(angle * M_PI) * radius;
	}
}

//#TPT-Directive ElementHeader Element_NEUT static int graphics(GRAPHICS_FUNC_ARGS)
int Element_NEUT::graphics(GRAPHICS_FUNC_ARGS)

{
	*firea = 120;
	*firer = 10;
	*fireg = 80;
	*fireb = 120;

	*pixel_mode |= FIRE_ADD;
	return 1;
}

//#TPT-Directive ElementHeader Element_NEUT static int DeutExplosion(Simulation * sim, int n, int x, int y, float temp, int t)
int Element_NEUT::DeutExplosion(Simulation * sim, int n, int x, int y, float temp, int t)//testing a new deut create part
{
	int i;
	n = (n/50);
	if (n < 1)
		n = 1;
	else if (n > 340)
		n = 340;

	for (int c = 0; c < n; c++)
	{
		i = sim->create_part(-3, x, y, t);
		if (i >= 0)
			sim->parts[i].temp = temp;
		else if (sim->pfree < 0)
			break;
	}
	sim->pv[y/CELL][x/CELL] += (6.0f * CFDS)*n;
	return 0;
}

Element_NEUT::~Element_NEUT() {}
