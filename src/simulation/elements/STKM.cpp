#include "simulation/Elements.h"
//#TPT-Directive ElementClass Element_STKM PT_STKM 55
Element_STKM::Element_STKM()
{
	Identifier = "DEFAULT_PT_STKM";
	Name = "STKM";
	Colour = PIXPACK(0xFFE0A0);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL;
	Enabled = 1;

	Advection = 0.5f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.2f;
	Loss = 1.0f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.0f;
	HotAir = 0.00f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 50;

	Temperature = R_TEMP+14.6f+273.15f;
	HeatConduct = 0;
	Description = "Stickman. Don't kill him! Control with the arrow keys.";

	Properties = PROP_NOCTYPEDRAW;
	Properties2 |= PROP_ALLOWS_WALL;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 620.0f;
	HighTemperatureTransition = PT_FIRE;

	Update = &Element_STKM::update;
	Graphics = &Element_STKM::graphics;
}

//#TPT-Directive ElementHeader Element_STKM static int lifeinc[4]
int Element_STKM::lifeinc[4] = {0,0,0,0};

//#TPT-Directive ElementHeader Element_STKM static char phase
char Element_STKM::phase = 0;

//#TPT-Directive ElementHeader Element_STKM static int update(UPDATE_FUNC_ARGS)
int Element_STKM::update(UPDATE_FUNC_ARGS)

{
	run_stickman(&sim->player, UPDATE_FUNC_SUBCALL_ARGS);
	return 0;
}



//#TPT-Directive ElementHeader Element_STKM static int graphics(GRAPHICS_FUNC_ARGS)
int Element_STKM::graphics(GRAPHICS_FUNC_ARGS)
{	
	*colr = *colg = *colb = *cola = 0;
	*pixel_mode = PSPEC_STICKMAN;
	return 1;
}

#define INBOND(x, y) ((x)>=0 && (y)>=0 && (x)<XRES && (y)<YRES)

//#TPT-Directive ElementHeader Element_STKM static int run_stickman(playerst *playerp, UPDATE_FUNC_ARGS)
int Element_STKM::run_stickman(playerst *playerp, UPDATE_FUNC_ARGS) {
	int r, rx, ry, ctype;
	int t = parts[i].type;

	float pp, d, pressure;
	float dt = 0.9;// /(FPSB*FPSB);  //Delta time in square
	float gvx, gvy;
	float gx, gy, dl, dr;
	float rocketBootsHeadEffect = 0.35f;
	float rocketBootsFeetEffect = 0.15f;
	float rocketBootsHeadEffectV = 0.3f;// stronger acceleration vertically, to counteract gravity
	float rocketBootsFeetEffectV = 0.45f;
	Particle *newpart = NULL;

	if (playerp->__flags & 2)
		playerp->__flags &= ~2;
	else if (parts[i].ctype && (sim->IsValidElement(parts[i].ctype) || parts[i].ctype == SPC_AIR || parts[i].ctype == SPC_VACUUM))
		STKM_set_element(sim, playerp, parts[i].ctype);
	playerp->frames++;
		
	//Temperature handling
	if (parts[i].temp<243 && !(sim->Extra_FIGH_pause & 16))
		parts[i].life -= 1;
	if ((parts[i].temp<309.6f) && (parts[i].temp>=243))
		parts[i].temp += 1;

	//Death
	pressure = sim->pv[y/CELL][x/CELL];
	if (pressure < 0.0f && (sim->Extra_FIGH_pause & 0x100))
	{
		pressure = -pressure;
	}
	if (parts[i].life<1 || (pressure>=4.5f && !(sim->Extra_FIGH_pause & 16) && (playerp->elem != SPC_AIR && playerp->elem != SPC_VACUUM)) ) //If his HP is less than 0 or there is very big wind...
	{
		if (playerp->elem != PT_FIGH)
		{
			for (r=-2; r<=1; r++)
			{
				sim->create_part(-1, x+r, y-2, playerp->elem);
				sim->create_part(-1, x+r+1, y+2, playerp->elem);
				sim->create_part(-1, x-2, y+r+1, playerp->elem);
				sim->create_part(-1, x+2, y+r, playerp->elem);
			}
		}
		else
			sim->create_part(-1, x, y, playerp->elem);
		sim->kill_part(i);  //Kill him
		return 1;
	}

	if (lifeinc[phase])
	{
		(lifeinc[phase] == 3) && (parts[i].life = 0);
		r = lifeinc[phase + 1];
		parts[i].life += r;
		(parts[i].life < 1) && (parts[i].life = (r > 0 ? INT_MAX : 0));
		sim->SimExtraFunc |= 0x1000;
	}

	//Follow gravity
	gvx = gvy = 0.0f;
	switch (sim->gravityMode)
	{
		default:
		case 0:
			gvy = 1;
			break;
		case 1:
			gvy = gvx = 0.0f;
			break;
		case 2:
			{
				float gravd;
				gravd = 0.01f - hypotf((parts[i].x - XCNTR), (parts[i].y - YCNTR));
				gvx = ((float)(parts[i].x - XCNTR) / gravd);
				gvy = ((float)(parts[i].y - YCNTR) / gravd);
			}
			break;
	}

	gvx += sim->gravx[((int)parts[i].y/CELL)*(XRES/CELL)+((int)parts[i].x/CELL)];
	gvy += sim->gravy[((int)parts[i].y/CELL)*(XRES/CELL)+((int)parts[i].x/CELL)];
	
	bool antigrav = sim->Extra_FIGH_pause & 0x200;
	int grav_multiplier = antigrav ? -1 : 1;

	if (antigrav) // anti-gravity ?
	{
		gvx = -gvx;
		gvy = -gvy;
	}

	float rbx = gvx;
	float rby = gvy;
	bool rbLowGrav = false;
	float tmp = fabsf(rbx) > fabsf(rby)?fabsf(rbx):fabsf(rby);
	if (tmp < 0.001f)
	{
		rbLowGrav = true;
		rbx = -parts[i].vx;
		rby = -parts[i].vy;
		tmp = fabsf(rbx) > fabsf(rby)?fabsf(rbx):fabsf(rby);
	}
	if (tmp < 0.001f)
	{
		rbx = 0;
		rby = 1.0f;
		tmp = 1.0f;
	}
	else
		tmp = 1.0f/sqrtf(rbx*rbx+rby*rby);
	rbx *= tmp;// scale to a unit vector
	rby *= tmp;
	if (rbLowGrav)
	{
		rocketBootsHeadEffectV = rocketBootsHeadEffect;
		rocketBootsFeetEffectV = rocketBootsFeetEffect;
	}

	parts[i].vx -= gvx*dt;  //Head up!
	parts[i].vy -= gvy*dt;

	//Verlet integration
	pp = 2*playerp->legs[0]-playerp->legs[2]+playerp->accs[0]*dt*dt;
	playerp->legs[2] = playerp->legs[0];
	playerp->legs[0] = pp;
	pp = 2*playerp->legs[1]-playerp->legs[3]+playerp->accs[1]*dt*dt;
	playerp->legs[3] = playerp->legs[1];
	playerp->legs[1] = pp;

	pp = 2*playerp->legs[4]-playerp->legs[6]+(playerp->accs[2]+gvx)*dt*dt;
	playerp->legs[6] = playerp->legs[4];
	playerp->legs[4] = pp;
	pp = 2*playerp->legs[5]-playerp->legs[7]+(playerp->accs[3]+gvy)*dt*dt;
	playerp->legs[7] = playerp->legs[5];
	playerp->legs[5] = pp;

	pp = 2*playerp->legs[8]-playerp->legs[10]+playerp->accs[4]*dt*dt;
	playerp->legs[10] = playerp->legs[8];
	playerp->legs[8] = pp;
	pp = 2*playerp->legs[9]-playerp->legs[11]+playerp->accs[5]*dt*dt;
	playerp->legs[11] = playerp->legs[9];
	playerp->legs[9] = pp;

	pp = 2*playerp->legs[12]-playerp->legs[14]+(playerp->accs[6]+gvx)*dt*dt;
	playerp->legs[14] = playerp->legs[12];
	playerp->legs[12] = pp;
	pp = 2*playerp->legs[13]-playerp->legs[15]+(playerp->accs[7]+gvy)*dt*dt;
	playerp->legs[15] = playerp->legs[13];
	playerp->legs[13] = pp;

	//Setting acceleration to 0
	playerp->accs[0] = 0;
	playerp->accs[1] = 0;

	playerp->accs[2] = 0;
	playerp->accs[3] = 0;

	playerp->accs[4] = 0;
	playerp->accs[5] = 0;

	playerp->accs[6] = 0;
	playerp->accs[7] = 0;

	gx = (playerp->legs[4] + playerp->legs[12])/2 - gvy;
	gy = (playerp->legs[5] + playerp->legs[13])/2 + gvx;
	dl = pow(gx - playerp->legs[4], 2) + pow(gy - playerp->legs[5], 2);
	dr = pow(gx - playerp->legs[12], 2) + pow(gy - playerp->legs[13], 2);

	//Go left
	if (((int)(playerp->comm)&0x01) == 0x01)
	{
		bool moved = false;
		if (dl>dr)
		{
			if (INBOND(playerp->legs[4], playerp->legs[5]) && !sim->eval_move(t, playerp->legs[4], playerp->legs[5], NULL))
			{
				playerp->accs[2] = -3*gvy-3*gvx;
				playerp->accs[3] = 3*gvx-3*gvy;
				playerp->accs[0] = -gvy;
				playerp->accs[1] = gvx;
				moved = true;
			}
		}
		else
		{
			if (INBOND(playerp->legs[12], playerp->legs[13]) && !sim->eval_move(t, playerp->legs[12], playerp->legs[13], NULL))
			{
				playerp->accs[6] = -3*gvy-3*gvx;
				playerp->accs[7] = 3*gvx-3*gvy;
				playerp->accs[0] = -gvy;
				playerp->accs[1] = gvx;
				moved = true;
			}
		}
		if (!moved && playerp->rocketBoots)
		{
			parts[i].vx -= rocketBootsHeadEffect*rby;
			parts[i].vy += rocketBootsHeadEffect*rbx;
			playerp->accs[2] -= rocketBootsFeetEffect*rby;
			playerp->accs[6] -= rocketBootsFeetEffect*rby;
			playerp->accs[3] += rocketBootsFeetEffect*rbx;
			playerp->accs[7] += rocketBootsFeetEffect*rbx;
			if (!(sim->Extra_FIGH_pause & 0x400))
			{
				for (int leg=0; leg<2; leg++)
				{
					if (leg==1 && (((int)(playerp->comm)&0x02) == 0x02))
						continue;
					int footX = playerp->legs[leg*8+4], footY = playerp->legs[leg*8+5];
					int np = sim->create_part(-1, footX, footY, PT_PLSM);
					if (np>=0)
					{
						parts[np].vx = parts[i].vx+rby*25;
						parts[np].vy = parts[i].vy-rbx*25;
						parts[np].life += 30;
					}
				}
			}
		}
	}

	//Go right
	if (((int)(playerp->comm)&0x02) == 0x02)
	{
		bool moved = false;
		if (dl<dr)
		{
			if (INBOND(playerp->legs[4], playerp->legs[5]) && !sim->eval_move(t, playerp->legs[4], playerp->legs[5], NULL))
			{
				playerp->accs[2] = 3*gvy-3*gvx;
				playerp->accs[3] = -3*gvx-3*gvy;
				playerp->accs[0] = gvy;
				playerp->accs[1] = -gvx;
				moved = true;
			}
		}
		else
		{
			if (INBOND(playerp->legs[12], playerp->legs[13]) && !sim->eval_move(t, playerp->legs[12], playerp->legs[13], NULL))
			{
				playerp->accs[6] = 3*gvy-3*gvx;
				playerp->accs[7] = -3*gvx-3*gvy;
				playerp->accs[0] = gvy;
				playerp->accs[1] = -gvx;
				moved = true;
			}
		}
		if (!moved && playerp->rocketBoots)
		{
			parts[i].vx += rocketBootsHeadEffect*rby;
			parts[i].vy -= rocketBootsHeadEffect*rbx;
			playerp->accs[2] += rocketBootsFeetEffect*rby;
			playerp->accs[6] += rocketBootsFeetEffect*rby;
			playerp->accs[3] -= rocketBootsFeetEffect*rbx;
			playerp->accs[7] -= rocketBootsFeetEffect*rbx;
			if (!(sim->Extra_FIGH_pause & 0x400))
			{
				for (int leg=0; leg<2; leg++)
				{
					if (leg==0 && (((int)(playerp->comm)&0x01) == 0x01))
						continue;
					int footX = playerp->legs[leg*8+4], footY = playerp->legs[leg*8+5];
					int np = sim->create_part(-1, footX, footY, PT_PLSM);
					if (np>=0)
					{
						parts[np].vx = parts[i].vx-rby*25;
						parts[np].vy = parts[i].vy+rbx*25;
						parts[np].life += 30;
					}
				}
			}
		}
	}

	if (playerp->rocketBoots && ((int)(playerp->comm)&0x03) == 0x03)
	{
		// Pressing left and right simultaneously with rocket boots on slows the stickman down
		// Particularly useful in zero gravity
		parts[i].vx *= 0.5f;
		parts[i].vy *= 0.5f;
		playerp->accs[2] = playerp->accs[6] = 0;
		playerp->accs[3] = playerp->accs[7] = 0;
	}

	//Jump
	if (((int)(playerp->comm)&0x04) == 0x04)
	{
		if (playerp->rocketBoots)
		{
			parts[i].vx -= rocketBootsHeadEffectV*rbx;
			parts[i].vy -= rocketBootsHeadEffectV*rby;
			playerp->accs[2] -= rocketBootsFeetEffectV*rbx;
			playerp->accs[6] -= rocketBootsFeetEffectV*rbx;
			playerp->accs[3] -= rocketBootsFeetEffectV*rby;
			playerp->accs[7] -= rocketBootsFeetEffectV*rby;
			if (!(sim->Extra_FIGH_pause & 0x400))
			{
				for (int leg=0; leg<2; leg++)
				{
					int footX = playerp->legs[leg*8+4], footY = playerp->legs[leg*8+5];
					int np = sim->create_part(-1, footX, footY+1, PT_PLSM);
					if (np>=0)
					{
						parts[np].vx = parts[i].vx+rbx*30;
						parts[np].vy = parts[i].vy+rby*30;
						parts[np].life += 10;
					}
				}
			}
		}
		else if ((INBOND(playerp->legs[4], playerp->legs[5]) && !sim->eval_move(t, playerp->legs[4], playerp->legs[5], NULL)) ||
				 (INBOND(playerp->legs[12], playerp->legs[13]) && !sim->eval_move(t, playerp->legs[12], playerp->legs[13], NULL)))
		{
			parts[i].vx -= 4*gvx;
			parts[i].vy -= 4*gvy;
			playerp->accs[2] -= gvx;
			playerp->accs[6] -= gvx;
			playerp->accs[3] -= gvy;
			playerp->accs[7] -= gvy;
		}
	}

	//Charge detector wall if foot inside
	if (INBOND((int)(playerp->legs[4]+0.5)/CELL, (int)(playerp->legs[5]+0.5)/CELL) &&
	       sim->bmap[(int)(playerp->legs[5]+0.5)/CELL][(int)(playerp->legs[4]+0.5)/CELL]==WL_DETECT)
		sim->set_emap((int)playerp->legs[4]/CELL, (int)playerp->legs[5]/CELL);
	if (INBOND((int)(playerp->legs[12]+0.5)/CELL, (int)(playerp->legs[13]+0.5)/CELL) &&
	        sim->bmap[(int)(playerp->legs[13]+0.5)/CELL][(int)(playerp->legs[12]+0.5)/CELL]==WL_DETECT)
		sim->set_emap((int)(playerp->legs[12]+0.5)/CELL, (int)(playerp->legs[13]+0.5)/CELL);

	int rndstore, randpool = 0, under_wall;
	//Searching for particles near head
	for (rx=-2; rx<3; rx++)
		for (ry=-2; ry<3; ry++)
			if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					r = sim->photons[y+ry][x+rx];

				if (!r && !sim->bmap[(y+ry)/CELL][(x+rx)/CELL])
					continue;
				
				if (!(sim->Extra_FIGH_pause & 32 || (r&0xFF) == PT_E186 && (playerp->__flags & 1)))
				{
					STKM_set_element(sim, playerp, r&0xFF);
				}
				if (!(sim->Extra_FIGH_pause & 16))
				{
					if ((r&0xFF) == PT_PLNT && parts[i].life<100) //Plant gives him 5 HP
					{
						if (parts[i].life<=95)
							parts[i].life += 5;
						else
							parts[i].life = 100;
						sim->kill_part(r>>8);
					}

					if ((r&0xFF) == PT_NEUT)
					{
						if (parts[i].life<=100) parts[i].life -= (102-parts[i].life)/2;
						else parts[i].life *= 0.9f;
						sim->kill_part(r>>8);
					}
				}

				if ((r&0xFF) == ELEM_MULTIPP)
				{
					if (!randpool)
					{
						randpool = 7;
						rndstore = rand(); // max refresh 4 times
					}
					if (!(rndstore & 3)) // condition: rand % 4 == 0 (actually 25% chance)
					{
						STKM_set_life_1(sim, r>>8, i);
					}
					rndstore >>= 2; randpool--;
					if (parts[r>>8].life == 27 && !(sim->Extra_FIGH_pause & 32))
					{
						ctype = parts[r>>8].ctype;
						int xctype = ctype >> 9;
						ctype &= 0x1FF;
						if (ctype && (sim->IsValidElement(ctype) || ctype == SPC_AIR || ctype == SPC_VACUUM || ctype == 0x102 || ctype == 0x103))
						{
							STKM_set_element(sim, playerp, ctype);
						}
						if (xctype == 1 || xctype == 2)
						{
							playerp->rocketBoots = xctype == 1 ? false : true;
						}
					}
				}

				under_wall = sim->bmap[(ry+y)/CELL][(rx+x)/CELL];
				switch (under_wall)
				{
				case WL_FAN:
					playerp->elem = (sim->Extra_FIGH_pause & 0x800) ? SPC_VACUUM : SPC_AIR;
					break;
				case WL_EHOLE:
					playerp->rocketBoots = false;
					break;
				case WL_GRAV: /* && parts[i].type!=PT_FIGH */
					playerp->rocketBoots = true;
					break;
				}
				if ((r&0xFF)==PT_PRTI)
					Element_STKM::STKM_interact(sim, playerp, i, rx, ry);
				if (!parts[i].type)//STKM_interact may kill STKM
					return 1;
			}

	//Head position
	rx = x + grav_multiplier * (3*((((int)playerp->pcomm)&0x02) == 0x02) - 3*((((int)playerp->pcomm)&0x01) == 0x01));
	ry = y - grav_multiplier * (3*(playerp->pcomm == 0));

	//Spawn
	if (((int)(playerp->comm)&0x08) == 0x08)
	{
		ry -= grav_multiplier * (2*(rand()%2)+1);
		r = pmap[ry][rx];
		if ((sim->elements[r&0xFF].Properties&TYPE_SOLID) && (r&0xFF) != ELEM_MULTIPP)
		{
			sim->create_part(-1, rx, ry, PT_SPRK);
			playerp->frames = 0;
		}
		else
		{
			int np = -1;
			int nelem = (playerp->__flags & 1) ? PT_E186 : playerp->elem;
			if (nelem == SPC_AIR || nelem == SPC_VACUUM)
			{
				for(int j = -4; j < 5; j++)
					for (int k = -4; k < 5; k++)
						sim->create_part(-1, rx + 3*((((int)playerp->pcomm)&0x02) == 0x02) - 3*((((int)playerp->pcomm)&0x01) == 0x01)+j, ry+k, nelem);
			}
			else if ((nelem == PT_LIGH || nelem == PT_FIGH) && playerp->frames<30)//limit lightning and fighter creation rate
				np = -1;
			else
				np = sim->create_part(-1, rx, ry, nelem);
			if ( (np < NPART) && np>=0)
			{
				if (nelem == PT_PHOT)
				{
					int random = abs(rand()%3-1)*3;
					if (random==0)
					{
						sim->kill_part(np);
					}
					else
					{
						parts[np].vy = 0;
						if (((int)playerp->pcomm)&(0x01|0x02))
							parts[np].vx = (((((int)playerp->pcomm)&0x02) == 0x02) - (((int)(playerp->pcomm)&0x01) == 0x01))*random;
						else
							parts[np].vx = random;
					}
				}
				else if (nelem == PT_E186)
				{
					newpart = &parts[np];
				}
				else if (nelem == PT_LIGH)
				{
					float angle;
					int power = 100;
					if (gvx!=0 || gvy!=0)
						angle = atan2(gvx, gvy)*180.0f/M_PI;
					else
						angle = rand()%360;
					if (((int)playerp->pcomm)&0x01)
						angle += 180;
					if (angle>360)
						angle-=360;
					if (angle<0)
						angle+=360;
					parts[np].tmp = angle;
					parts[np].life=rand()%(2+power/15)+power/7;
					parts[np].temp=parts[np].life*power/2.5;
					parts[np].tmp2=1;
				}
				else if (nelem != SPC_AIR && nelem != SPC_VACUUM)
				{
					parts[np].vx -= -gvy*(5*((((int)playerp->pcomm)&0x02) == 0x02) - 5*(((int)(playerp->pcomm)&0x01) == 0x01));
					parts[np].vy -= gvx*(5*((((int)playerp->pcomm)&0x02) == 0x02) - 5*(((int)(playerp->pcomm)&0x01) == 0x01));
					parts[i].vx -= (sim->elements[(int)nelem].Weight*parts[np].vx)/1000;
					if (nelem == PT_FIGH)
						createSTKMChild(sim, playerp, i, np);
				}
				playerp->frames = 0;
			}

		}
	}

	//Simulation of joints
	d = 25/(pow((playerp->legs[0]-playerp->legs[4]), 2) + pow((playerp->legs[1]-playerp->legs[5]), 2)+25) - 0.5;  //Fast distance
	playerp->legs[4] -= (playerp->legs[0]-playerp->legs[4])*d;
	playerp->legs[5] -= (playerp->legs[1]-playerp->legs[5])*d;
	playerp->legs[0] += (playerp->legs[0]-playerp->legs[4])*d;
	playerp->legs[1] += (playerp->legs[1]-playerp->legs[5])*d;

	d = 25/(pow((playerp->legs[8]-playerp->legs[12]), 2) + pow((playerp->legs[9]-playerp->legs[13]), 2)+25) - 0.5;
	playerp->legs[12] -= (playerp->legs[8]-playerp->legs[12])*d;
	playerp->legs[13] -= (playerp->legs[9]-playerp->legs[13])*d;
	playerp->legs[8] += (playerp->legs[8]-playerp->legs[12])*d;
	playerp->legs[9] += (playerp->legs[9]-playerp->legs[13])*d;

	d = 36/(pow((playerp->legs[0]-parts[i].x), 2) + pow((playerp->legs[1]-parts[i].y), 2)+36) - 0.5;
	parts[i].vx -= (playerp->legs[0]-parts[i].x)*d;
	parts[i].vy -= (playerp->legs[1]-parts[i].y)*d;
	playerp->legs[0] += (playerp->legs[0]-parts[i].x)*d;
	playerp->legs[1] += (playerp->legs[1]-parts[i].y)*d;

	d = 36/(pow((playerp->legs[8]-parts[i].x), 2) + pow((playerp->legs[9]-parts[i].y), 2)+36) - 0.5;
	parts[i].vx -= (playerp->legs[8]-parts[i].x)*d;
	parts[i].vy -= (playerp->legs[9]-parts[i].y)*d;
	playerp->legs[8] += (playerp->legs[8]-parts[i].x)*d;
	playerp->legs[9] += (playerp->legs[9]-parts[i].y)*d;

	if (INBOND(playerp->legs[4], playerp->legs[5]) && !sim->eval_move(t, playerp->legs[4], playerp->legs[5], NULL))
	{
		playerp->legs[4] = playerp->legs[6];
		playerp->legs[5] = playerp->legs[7];
	}

	if (INBOND(playerp->legs[12], playerp->legs[13]) && !sim->eval_move(t, playerp->legs[12], playerp->legs[13], NULL))
	{
		playerp->legs[12] = playerp->legs[14];
		playerp->legs[13] = playerp->legs[15];
	}

	//This makes stick man "pop" from obstacles
	if (INBOND(playerp->legs[4], playerp->legs[5]) && !sim->eval_move(t, playerp->legs[4], playerp->legs[5], NULL))
	{
		float t;
		t = playerp->legs[4]; playerp->legs[4] = playerp->legs[6]; playerp->legs[6] = t;
		t = playerp->legs[5]; playerp->legs[5] = playerp->legs[7]; playerp->legs[7] = t;
	}

	if (INBOND(playerp->legs[12], playerp->legs[13]) && !sim->eval_move(t, playerp->legs[12], playerp->legs[13], NULL))
	{
		float t;
		t = playerp->legs[12]; playerp->legs[12] = playerp->legs[14]; playerp->legs[14] = t;
		t = playerp->legs[13]; playerp->legs[13] = playerp->legs[15]; playerp->legs[15] = t;
	}

	//Keeping legs distance
	if ((pow((playerp->legs[4] - playerp->legs[12]), 2) + pow((playerp->legs[5]-playerp->legs[13]), 2))<16)
	{
		float tvx, tvy;
		tvx = -gvy;
		tvy = gvx;

		if (tvx || tvy)
		{
			playerp->accs[2] -= 0.2*tvx/hypot(tvx, tvy);
			playerp->accs[3] -= 0.2*tvy/hypot(tvx, tvy);

			playerp->accs[6] += 0.2*tvx/hypot(tvx, tvy);
			playerp->accs[7] += 0.2*tvy/hypot(tvx, tvy);
		}
	}

	if ((pow((playerp->legs[0] - playerp->legs[8]), 2) + pow((playerp->legs[1]-playerp->legs[9]), 2))<16)
	{
		float tvx, tvy;
		tvx = -gvy;
		tvy = gvx;

		if (tvx || tvy)
		{
			playerp->accs[0] -= 0.2*tvx/hypot(tvx, tvy);
			playerp->accs[1] -= 0.2*tvy/hypot(tvx, tvy);

			playerp->accs[4] += 0.2*tvx/hypot(tvx, tvy);
			playerp->accs[5] += 0.2*tvy/hypot(tvx, tvy);
		}
	}

	//If legs touch something
	Element_STKM::STKM_interact(sim, playerp, i, (int)(playerp->legs[4]+0.5), (int)(playerp->legs[5]+0.5));
	Element_STKM::STKM_interact(sim, playerp, i, (int)(playerp->legs[12]+0.5), (int)(playerp->legs[13]+0.5));
	Element_STKM::STKM_interact(sim, playerp, i, (int)(playerp->legs[4]+0.5), (int)playerp->legs[5]);
	Element_STKM::STKM_interact(sim, playerp, i, (int)(playerp->legs[12]+0.5), (int)playerp->legs[13]);
	if (!parts[i].type)
		return 1;

	if (playerp->elem == PT_FIGH && !(sim->Extra_FIGH_pause & 8))
		playerp->elem = playerp->pelem;
	parts[i].ctype = playerp->elem;
	
	if (newpart)
	{
		int rndstore = rand();
		if (!rbLowGrav)
		{
			float rad = 3.0f;
			float angle = (rndstore % 101 - 50) * 0.002f + atan2f(-gvx, gvy);
			int comm = (int)playerp->pcomm;
			if ((comm & 3) == 1 || (comm & 3) != 2 && (rndstore % 2))
				rad = -rad;
			newpart->vx = rad*cosf(angle);
			newpart->vy = rad*sinf(angle);
		}
		else
		{
			newpart->vx *= 1.5f;
			newpart->vy *= 1.5f;
		}
		newpart->ctype = playerp->elem;
		if (newpart->ctype >= 0x100)
			newpart->ctype = 0;
	}
	return 0;
}

//#TPT-Directive ElementHeader Element_STKM static void STKM_interact(Simulation *sim, playerst *playerp, int i, int x, int y)
void Element_STKM::STKM_interact(Simulation *sim, playerst *playerp, int i, int x, int y)
{
	int r;
	if (x<0 || y<0 || x>=XRES || y>=YRES || !sim->parts[i].type)
		return;
	r = sim->pmap[y][x];
	if (r)
	{
		if ((r&0xFF) == PT_PINVIS)
			r = sim->parts[r>>8].tmp4;
		if (!(sim->Extra_FIGH_pause & 16))
		{
			if ((r&0xFF)==PT_SPRK && playerp->elem!=PT_LIGH) //If on charge
			{
				sim->parts[i].life -= (int)(rand()*20/RAND_MAX)+32;
			}

			if (sim->elements[r&0xFF].HeatConduct && ((r&0xFF)!=PT_HSWC||sim->parts[r>>8].life==10) && ((playerp->elem!=PT_LIGH && sim->parts[r>>8].temp>=323) || sim->parts[r>>8].temp<=243) && (!playerp->rocketBoots || (r&0xFF)!=PT_PLSM))
			{
				sim->parts[i].life -= 2;
				playerp->accs[3] -= 1;
			}
				
			if (sim->elements[r&0xFF].Properties&PROP_DEADLY)
				switch (r&0xFF)
				{
					case PT_ACID:
						sim->parts[i].life -= 5;
						break;
					default:
						sim->parts[i].life -= 1;
						break;
				}

			if (sim->elements[r&0xFF].Properties&PROP_RADIOACTIVE)
				sim->parts[i].life -= 1;
		}
		
		if ((r&0xFF) == ELEM_MULTIPP)
		{
			STKM_set_life_1(sim, r>>8, i);
			if (sim->parts[r>>8].life == 23)
			{
				sim->parts[i].vy -= 3;
				playerp->accs[3] -= 3;
				playerp->accs[7] -= 3;
			}
		}

		if ((r&0xFF)==PT_PRTI && sim->parts[i].type)
		{
			int nnx, count=1;//gives rx=0, ry=1 in update_PRTO
			sim->parts[r>>8].tmp = (int)((sim->parts[r>>8].temp-73.15f)/100+1);
			if (sim->parts[r>>8].tmp>=CHANNELS) sim->parts[r>>8].tmp = CHANNELS-1;
			else if (sim->parts[r>>8].tmp<0) sim->parts[r>>8].tmp = 0;
			for (nnx=0; nnx<80; nnx++)
				if (!sim->portalp[sim->parts[r>>8].tmp][count][nnx].type)
				{
					sim->portalp[sim->parts[r>>8].tmp][count][nnx] = sim->parts[i];
					sim->kill_part(i);
					//stop new STKM/fighters being created to replace the ones in the portal:
					playerp->spwn = 1;
					if (sim->portalp[sim->parts[r>>8].tmp][count][nnx].type==PT_FIGH)
						sim->fighcount++;
					break;
				}
		}
		if (((r&0xFF)==PT_BHOL || (r&0xFF)==PT_NBHL) && sim->parts[i].type)
		{
			if (!sim->legacy_enable)
			{
				sim->parts[r>>8].temp = restrict_flt(sim->parts[r>>8].temp+sim->parts[i].temp/2, MIN_TEMP, MAX_TEMP);
			}
			sim->kill_part(i);
		}
		if (((r&0xFF)==PT_VOID || ((r&0xFF)==PT_PVOD && sim->parts[r>>8].life==10)) && (!sim->parts[r>>8].ctype || (sim->parts[r>>8].ctype==sim->parts[i].type)!=(sim->parts[r>>8].tmp&1)) && sim->parts[i].type)
		{
			sim->kill_part(i);
		}
	}
}

//#TPT-Directive ElementHeader Element_STKM static void STKM_init_legs(Simulation * sim, playerst *playerp, int i)
void Element_STKM::STKM_init_legs(Simulation * sim, playerst *playerp, int i)
{
	int x, y;

	x = (int)(sim->parts[i].x+0.5f);
	y = (int)(sim->parts[i].y+0.5f);

	playerp->legs[0] = x-1;
	playerp->legs[1] = y+6;
	playerp->legs[2] = x-1;
	playerp->legs[3] = y+6;

	playerp->legs[4] = x-3;
	playerp->legs[5] = y+12;
	playerp->legs[6] = x-3;
	playerp->legs[7] = y+12;

	playerp->legs[8] = x+1;
	playerp->legs[9] = y+6;
	playerp->legs[10] = x+1;
	playerp->legs[11] = y+6;

	playerp->legs[12] = x+3;
	playerp->legs[13] = y+12;
	playerp->legs[14] = x+3;
	playerp->legs[15] = y+12;

	for (int j = 0; j < 8; j++)
		playerp->accs[j] = 0;
	playerp->comm = 0;
	playerp->pcomm = 0;
	playerp->frames = 0;
	playerp->__flags = 0;
	playerp->parentStickman = -1;
	playerp->firstChild = -1;
	playerp->prevStickman = -1;
	playerp->nextStickman = -1;
	playerp->lastChild = -1;
	playerp->self_ID = i;
	playerp->underp = 0;
}

//#TPT-Directive ElementHeader Element_STKM static void STKM_set_element(Simulation *sim, playerst *playerp, int element)
void Element_STKM::STKM_set_element(Simulation *sim, playerst *playerp, int element)
{
	if (element == 0x102 || element == 0x103)
	{
		playerp->__flags &= ~0x1;
		playerp->__flags |= (element == 0x102) ? 1 : 0;
	}
	else if (sim->elements[element].Falldown != 0
	    || sim->elements[element].Properties&TYPE_GAS
	    || sim->elements[element].Properties&TYPE_LIQUID
	    || sim->elements[element].Properties&TYPE_ENERGY
	    || element == PT_LOLZ || element == PT_LOVE || element == SPC_AIR || element == SPC_VACUUM)
	{
		if (!playerp->rocketBoots || element != PT_PLSM)
			playerp->elem = element;
	}
	if (element == PT_TESC || element == PT_LIGH)
		playerp->elem = PT_LIGH;
	if (playerp->elem != PT_FIGH && element == PT_FIGH && (sim->Extra_FIGH_pause & 8))
	{
		playerp->pelem = playerp->elem;
		playerp->elem = PT_FIGH;
	}
}

//#TPT-Directive ElementHeader Element_STKM static void STKM_set_life_1(Simulation *sim, int s, int i)
void Element_STKM::STKM_set_life_1(Simulation *sim, int s, int i)
{
	if (sim->parts[s].life == 16 && sim->parts[s].ctype == 5)
	{
		int sur_part_tmp = sim->parts[s].tmp >> 6;
		if (sur_part_tmp > 0)
		{
			sur_part_tmp--; // is temporary variable, not global
			int inc_life = (sur_part_tmp & 1) ? 1 : -1;
			bool inc_life_cond;
			switch (sur_part_tmp >> 1)
			{
				case 0:
					inc_life_cond = true;
				break;
				case 1:
					inc_life_cond = (sim->parts[i].type == PT_STKM);
				break;
				case 2:
					inc_life_cond = (sim->parts[i].type == PT_STKM2);
				break;
				case 3:
				{
					int part_i_type = sim->parts[i].type;
					inc_life_cond = (part_i_type == PT_STKM || part_i_type == PT_STKM2);
				}
				break;
				case 4:
					inc_life_cond = (sim->parts[i].type == PT_FIGH);
				break;
			}
			if (inc_life_cond)
			{
				if (inc_life < 0 || sim->parts[i].life < 100)
					sim->parts[i].life += inc_life;
			}
		}
	}
}

//#TPT-Directive ElementHeader Element_STKM static void removeSTKMChilds(Simulation *sim, playerst* playerp)
void Element_STKM::removeSTKMChilds(Simulation *sim, playerst* playerp)
{
	int child_f = playerp->firstChild;
	while (child_f >= 0)
	{
		sim->fighters[child_f].parentStickman = -1;
		child_f = sim->fighters[child_f].nextStickman;
	}
}

//#TPT-Directive ElementHeader Element_STKM static void createSTKMChild(Simulation *sim, playerst* playerp, int i, int np)
void Element_STKM::createSTKMChild(Simulation *sim, playerst* playerp, int i, int np)
{
	int old_FIGH_id, new_FIGH_id;
	old_FIGH_id = playerp->lastChild;
	new_FIGH_id = sim->parts[np].tmp;
	playerp->lastChild = new_FIGH_id;
	if (playerp->firstChild < 0)
		playerp->firstChild = new_FIGH_id;
	else {
		sim->fighters[new_FIGH_id].prevStickman = old_FIGH_id;
		sim->fighters[old_FIGH_id].nextStickman = new_FIGH_id;
	}
	if (sim->parts[i].type == PT_FIGH)
		sim->fighters[new_FIGH_id].parentStickman = sim->parts[i].tmp;
	else if (sim->parts[i].type == PT_STKM)
		sim->fighters[new_FIGH_id].parentStickman = MAX_FIGHTERS;
	else if (sim->parts[i].type == PT_STKM2)
		sim->fighters[new_FIGH_id].parentStickman = MAX_FIGHTERS + 1;
}

Element_STKM::~Element_STKM() {}
