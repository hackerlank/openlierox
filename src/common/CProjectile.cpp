/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Projectile Class
// Created 11/2/02
// Jason Boettcher


#include "LieroX.h"
#include "GfxPrimitives.h"
#include "CProjectile.h"
#include "Protocol.h"
#include "CWorm.h"
#include "Entity.h"
#include "MathLib.h"
#include "CClient.h"


///////////////////
// Spawn the projectile
void CProjectile::Spawn(proj_t *_proj, CVec _pos, CVec _vel, int _rot, int _owner, int _random, bool _remote, float _remotetime)
{
	tProjInfo = _proj;
	fLife = 0;
	fExtra = 0;
	vOldPos = _pos;
	vPosition = _pos;
	vVelocity = _vel;
	fRotation = (float)_rot;
	iOwner = _owner;
	bUsed = true;
	iSpawnPrjTrl = false;
	fLastTrailProj = -99999;
	iRandom = _random;
	bRemote = _remote;
    iFrameX = 0;
	fLastSimulationTime = tLX->fCurTime;
	
    fTimeVarRandom = GetFixedRandomNum(iRandom);

	if(bRemote)
		fRemoteFrameTime = _remotetime;

	// this produce a memory leak
	fSpeed = _vel.GetLength();

	fFrame = 0;
	iFrameDelta = true;

	firstbounce = true;

	// Choose a colour
	if(tProjInfo->Type == PRJ_PIXEL) {
		int c = GetRandomInt(tProjInfo->NumColours-1);
		if(c == 0)
			iColour = MakeColour(tProjInfo->Colour1[0], tProjInfo->Colour1[1], tProjInfo->Colour1[2]);
		else if(c==1)
			iColour = MakeColour(tProjInfo->Colour2[0], tProjInfo->Colour2[1], tProjInfo->Colour2[2]);
	}

    // Events
    nExplode = false;
    nTouched = false;
}


///////////////////
// Gets a random float from a special list
float CProjectile::getRandomFloat(void)
{
	float r = GetFixedRandomNum(iRandom++);

	iRandom %= 255;

	return r;
}



///////////////////
// Check for a collision
// Returns:
// -1 if some collision
// -1000 if none
// >=0 is collision with worm (the return-value is the ID) 
// TODO: we need one single CheckCollision which is used everywhere in the code
// atm we have 2 CProj::CC, Map:CC and ProjWormColl and fastTraceLine
// we should complete the function in CMap.cpp in a general way by using fastTraceLine
// also dt shouldn't be a parameter, you should specify a start- and an endpoint
// (for example CWorm_AI also uses this to check some possible cases)
int CProjectile::CheckCollision(float dt, CMap *map, CWorm* worms, float* enddt)
{
	static const int NONE_COL_RET = -1000;
	static const int SOME_COL_RET = -1;
	
	int MIN_CHECKSTEP = 4; // only after a step of this, the check for a collision will be made
	int MAX_CHECKSTEP = 6; // if step is wider than this, it will be intersected
	int AVG_CHECKSTEP = 4; // this is used for the intersection, if the step is to wide
	static const int WORM_CHECKSTEP = 2; // this is used for worm collisions
	int len = (int)vVelocity.GetLength2();
	
	// HINT: this prevents napalms and similar stuff from flying through walls (see TODO according to MIN_CHECKSTEP below)
	if (tProjInfo->Dampening <= 1) // This rule does not apply to "accelerating" projectiles
		len = MIN(tProjInfo->ProjSpeed + (int)(tProjInfo->ProjSpeedVar * iRandom), len);

	
	if (len < 14000)  {
		MIN_CHECKSTEP = 0;
		MAX_CHECKSTEP = 3;
		AVG_CHECKSTEP = 2;
	} else if (len >= 14000 && len < 75000)  {
		MIN_CHECKSTEP = 1;
		MAX_CHECKSTEP = 4;
		AVG_CHECKSTEP = 2;
	} else if (len >= 75000 && len < 250000)  {
		MIN_CHECKSTEP = 1;
		MAX_CHECKSTEP = 5;
		AVG_CHECKSTEP = 2;

	// HINT: add or substract some small random number for high speeds, it behaves more like original LX
	} else if (len >= 250000)  {
		int rnd = GetRandomInt(2)*SIGN(GetRandomNum());
		MIN_CHECKSTEP = 6;
		MAX_CHECKSTEP = 9 + rnd;
		AVG_CHECKSTEP = 6 + rnd;
	}
	
	// Check if it hit the terrain
	int mw = map->GetWidth();
	int mh = map->GetHeight();
	int w,h;
	int px,py,x,y;
	int ret;
	
	if(tProjInfo->Type == PRJ_PIXEL)
		w=h=1;
	else
		w=h=2;

	CVec newvel = vVelocity;
	// Gravity
	if(tProjInfo->UseCustomGravity)
		newvel.y += (float)(tProjInfo->Gravity)*dt;
	else
		newvel.y += 100*dt;	
	
	// Dampening
	if(tProjInfo->Dampening != 1)
		newvel *= (float)pow(tProjInfo->Dampening, dt*55); // TODO: is this good?
	
	float checkstep = newvel.GetLength2(); // |v|^2
	if(( checkstep*dt*dt > MAX_CHECKSTEP*MAX_CHECKSTEP )) { // |dp|^2=|v*dt|^2
		// calc new dt, so that we have |v*dt|=AVG_CHECKSTEP
		// checkstep is new dt
		checkstep = (float)AVG_CHECKSTEP / sqrt(checkstep);
		
		for(float time = 0; time < dt; time += checkstep) {
			ret = CheckCollision(time+checkstep>dt ? dt-time : checkstep, map,worms,enddt);
			if(ret >= -1) {
				if(enddt) *enddt += time;
				return ret;
			}
		}
		
		if(enddt) *enddt = dt;
		return NONE_COL_RET;
	}

	vVelocity = newvel;
	if(enddt) *enddt = dt;
	vPosition += vVelocity*dt;
	px=(int)(vPosition.x);
	py=(int)(vPosition.y);

	// got we any worm?
	if((checkstep*dt*dt > WORM_CHECKSTEP*WORM_CHECKSTEP))  {
		checkstep = WORM_CHECKSTEP / sqrt(checkstep);
		CVec curpos = vOldPos;
		for (float time=0; time < dt; time += checkstep)  {
			ret = ProjWormColl(curpos,worms);
			if (ret >= 0)  {
				if (enddt) *enddt = time;
				return ret;
			}
			curpos += vVelocity*checkstep;
		}
	}

	ret = ProjWormColl(vPosition, worms);
	if(ret >= 0) {
		return ret;
	}
	
	// if distance is to short to last check, just return here without a check
	// TODO: this is not so good because if the projectile is flying quite slow
	// and is already in wall, the collision check won't be made at all and the
	// projectile will simply fly through
	// I "fixed" this by setting MIN_CHECKSTEP to 0 for slow projectiles but it would
	// be nicer to have it correct
	if( (vOldPos-vPosition).GetLength2() < MIN_CHECKSTEP*MIN_CHECKSTEP )
		return NONE_COL_RET;

	CollisionSide = 0;
	short top,bottom,left,right;
	top=bottom=left=right=0;

	// Hit edges
	if(px-w<0 || py-h<0 || px+w>=mw || py+h>=mh) {

		// Check the collision side
		if(px-w<0) {
			px = w;
			CollisionSide |= COL_LEFT;
		}
		if(py-h<0) {
			py = h;
			CollisionSide |= COL_TOP;
		}
		if(px+w>=mw) {
			px = mw-w;
			CollisionSide |= COL_RIGHT;
		}
		if(py+h>=mh) {
			py = mh-h;
			CollisionSide |= COL_BOTTOM;
		}

		//vPosition.x = (float)px;
		//vPosition.y = (float)py;
		vPosition = vOldPos;
		return SOME_COL_RET;
	}

	const uchar* gridflags = map->getAbsoluteGridFlags();
	int grid_w = map->getGridWidth();
	int grid_h = map->getGridHeight();
	int grid_cols = map->getGridCols();
	if(grid_w < 2*w+1 || grid_h < 2*h+1 // this ensures, that this check is safe
	|| gridflags[((py-h)/grid_h)*grid_cols + (px-w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py+h)/grid_h)*grid_cols + (px-w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py-h)/grid_h)*grid_cols + (px+w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py+h)/grid_h)*grid_cols + (px+w)/grid_w] & (PX_ROCK|PX_DIRT))
	for(y=py-h;y<=py+h;y++) {

		// Clipping means that it has collided
		if(y<0)	{
			CollisionSide |= COL_TOP;
			vPosition = vOldPos;
			return SOME_COL_RET;
		}
		if(y>=mh) {
			CollisionSide |= COL_BOTTOM;
			vPosition = vOldPos;
			return SOME_COL_RET;
		}


		const uchar *pf = map->GetPixelFlags() + y*mw + px-w;

		for(x=px-w;x<=px+w;x++) {

			// Clipping
			if(x<0) {
				CollisionSide |= COL_LEFT;
				vPosition = vOldPos;
				return SOME_COL_RET;
			}
			if(x>=mw) {
				CollisionSide |= COL_RIGHT;
				vPosition = vOldPos;
				return SOME_COL_RET;
			}

			if(*pf & PX_DIRT || *pf & PX_ROCK) {
				if(y<py)
					top++;
				else if(y>py)
					bottom++;
				if(x<px)
					left++;
				else if(x>px)
					right++;
			}

			pf++;
		}
	}


	// Check for a collision
	if(top || bottom || left || right) {
		CollisionSide = 0;

		if(tProjInfo->Hit_Type == PJ_EXPLODE) {
			// HINT: don't reset vPosition here, because we want
			//		the explosion near (inside) the object
			//		this behavior is the same as in original LX
			return SOME_COL_RET;
		}

		// HINT: this calcs only the last step, not vOldPos
		//		it's more like the original LX if we do this
		CVec pos = vPosition - vVelocity*dt; // old pos
		bool bounce = false;
		
		// Bit of a hack
		if( tProjInfo->Hit_Type == PJ_BOUNCE ) {
			// HINT: don't reset vPosition here; it will be reset,
			//		depending on the collisionside
			bounce = true;
		} else if ( tProjInfo->Hit_Type == PJ_NOTHING )  {  // PJ_NOTHING projectiles go through walls (but a bit slower)
			vPosition -= (vVelocity*dt)*0.8f;				// Note: the speed in walls could be moddable
			vOldPos = vPosition;
		} else
			vPosition = vOldPos;
							
		// Find the collision side
		if( (left>right || left>2) && left>1 && vVelocity.x < 0) {
			if(bounce)
				vPosition.x=( pos.x );
			CollisionSide |= COL_LEFT;
		}

		if( (right>left || right>2) && right>1 && vVelocity.x > 0) {
			if(bounce)
				vPosition.x=( pos.x );
			CollisionSide |= COL_RIGHT;
		}

		if(top>1 && vVelocity.y < 0) {
			if(bounce)
				vPosition.y=( pos.y );
			CollisionSide |= COL_TOP;
		}

		if(bottom>1 && vVelocity.y > 0) {
			if(bounce)
				vPosition.y=( pos.y );
			CollisionSide |= COL_BOTTOM;
		}

		// If the velocity is too low, just stop me
		if(fabs(vVelocity.x) < 2)
			vVelocity.x=0;
		if(fabs(vVelocity.y) < 2)
			vVelocity.y=0;
						
		return SOME_COL_RET;
	}
	
	// the move was save, so save the position
	vOldPos = vPosition;
	return NONE_COL_RET;
}

///////////////////
// Check for a collision (static version; doesnt do anything else then checking)
// Returns true if there was a collision, otherwise false is returned
int CProjectile::CheckCollision(proj_t* tProjInfo, float dt, CMap *map, CVec pos, CVec vel)
{
	// Check if it hit the terrain
	int mw = map->GetWidth();
	int mh = map->GetHeight();
	int w,h;
	int px,py,x,y;

	if(tProjInfo->Type == PRJ_PIXEL)
		w=h=1;
	else
		w=h=2;

	float maxspeed2 = (float)(4*w*w+4*w+1); // (2w+1)^2
	if( (vel*dt).GetLength2() > maxspeed2) {
		dt *= 0.5f;

		if(CheckCollision(tProjInfo,dt,map,pos,vel))
			return true;

		pos += vel*dt;

		return CheckCollision(tProjInfo,dt,map,pos,vel);
	}

	pos += vel*dt;

	px=(int)pos.x;
	py=(int)pos.y;

	short top,bottom,left,right;
	top=bottom=left=right=0;

	// Hit edges
	if(px-w<0 || py-h<0 || px+w>=mw || py+h>=mh) {

		return true;
	}

	const uchar* gridflags = map->getAbsoluteGridFlags();
	int grid_w = map->getGridWidth();
	int grid_h = map->getGridHeight();
	int grid_cols = map->getGridCols();
	if(grid_w < 2*w+1 || grid_h < 2*h+1 // this ensures, that this check is safe
	|| gridflags[((py-h)/grid_h)*grid_cols + (px-w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py+h)/grid_h)*grid_cols + (px-w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py-h)/grid_h)*grid_cols + (px+w)/grid_w] & (PX_ROCK|PX_DIRT)
	|| gridflags[((py+h)/grid_h)*grid_cols + (px+w)/grid_w] & (PX_ROCK|PX_DIRT))
	for(y=py-h;y<=py+h;y++) {

		// Clipping means that it has collided
		if(y<0 || y>=mh)	{
			return true;
		}

		const uchar *pf = map->GetPixelFlags() + y*mw + px-w;

		for(x=px-w;x<=px+w;x++) {

			// Clipping
			if(x<0 || x>=mw) {
				return true;
			}

			if(*pf & PX_DIRT || *pf & PX_ROCK) {
				if(y<py)
					top++;
				else if(y>py)
					bottom++;
				if(x<px)
					left++;
				else if(x>px)
					right++;
			}

			pf++;
		}
	}


	// Check for a collision
	if(top || bottom || left || right) {
		return true;
	}

	return false;
}

///////////////////
// Draw the projectile
void CProjectile::Draw(SDL_Surface *bmpDest, CViewport *view)
{
	int wx = view->GetWorldX();
	int wy = view->GetWorldY();
	int l = view->GetLeft();
	int t = view->GetTop();
	float framestep;

	int x=((int)vPosition.x-wx)*2+l;
	int y=((int)vPosition.y-wy)*2+t;

	// Clipping on the viewport
	if((x<l || x>l+view->GetVirtW()))
		return;
	if((y<t || y>t+view->GetVirtH()))
		return;

    if(tProjInfo->Type == PRJ_PIXEL) {
		DrawRectFill(bmpDest,x-1,y-1,x+1,y+1,iColour);
        return;
    }
	else if(tProjInfo->Type == PRJ_IMAGE) {
		if(tProjInfo->bmpImage == NULL)
			return;

		// Spinning projectile only when moving
		if(tProjInfo->Rotating && (vVelocity.x != 0 || vVelocity.y != 0))
			framestep = fRotation / (float)tProjInfo->RotIncrement;
		else
			framestep = 0;

		// Directed in the direction the projectile is travelling
		if(tProjInfo->UseAngle) {
			CVec dir = vVelocity;
			float angle = (float)( -atan2(dir.x,dir.y) * (180.0f/PI) );
			float offset = 360.0f / (float)tProjInfo->AngleImages;

			if(angle < 0)
				angle+=360;
			if(angle > 360)
				angle-=360;

			if(angle == 360)
				angle=0;

			framestep = angle / offset;
		}

		// Special angle
		// Basically another way of organising the angles in images
		// Straight up is in the middle, rotating left goes left, rotating right goes right in terms
		// of image index's from the centre
		if(tProjInfo->UseSpecAngle) {
			CVec dir = vVelocity;
			float angle = (float)( -atan2(dir.x,dir.y) * (180.0f/PI) );
			int direct = 0;

			if(angle > 0)
				angle=180-angle;
			if(angle < 0) {
				angle=180+angle;
				direct = 1;
			}
			if(angle == 0)
				direct = 0;


			int num = (tProjInfo->AngleImages - 1) / 2;
			if(direct == 0)
				// Left side
				framestep = (float)(151-angle) / 151.0f * (float)num;
			else {
				// Right side
				framestep = (float)angle / 151.0f * (float)num;
				framestep += num+1;
			}
		}

		if(tProjInfo->Animating)
			framestep = fFrame;

		int size = tProjInfo->bmpImage->h;
		int half = size/2;
        iFrameX = (int)framestep*size;

		DrawImageAdv(bmpDest, tProjInfo->bmpImage, (int)framestep*size, 0, x-half, y-half, size,size);
	}
}


///////////////////
// Draw the projectiles shadow
void CProjectile::DrawShadow(SDL_Surface *bmpDest, CViewport *view, CMap *map)
{
	int wx = view->GetWorldX();
	int wy = view->GetWorldY();
	int l = view->GetLeft();
	int t = view->GetTop();

	int x=((int)vPosition.x-wx)*2+l;
	int y=((int)vPosition.y-wy)*2+t;

	// Clipping on the viewport
	if((x<l || x>l+view->GetVirtW()))
		return;
	if((y<t || y>t+view->GetVirtH()))
		return;

	// Pixel
	if(tProjInfo->Type == PRJ_PIXEL)
		map->DrawPixelShadow(bmpDest, view, (int)vPosition.x, (int)vPosition.y);

	// Image
	else if(tProjInfo->Type == PRJ_IMAGE) {
		if(tProjInfo->bmpImage == NULL)
			return;

	int size = tProjInfo->bmpImage->h;
	int half = size/2;
	map->DrawObjectShadow(bmpDest, tProjInfo->bmpImage, iFrameX, 0, size,size, view, (int)vPosition.x-(half>>1), (int)vPosition.y-(half>>1));
    }
}


///////////////////
// Bounce
// HINT: this is not exactly the way the original LX did it,
//		but this way is way more correct and it seems to work OK
//		(original LX resets the bounce-direction on each checked side)
void CProjectile::Bounce(float fCoeff)
{	
	float x,y;
	x=y=1;

	// This code is right, it should be done like that
	// However, we want to keep compatibility with .56 and when on each client would be another simulation,
	// we couldn't call that compatibility at all

	// For now we just keep the old, wrong code, so noone will call OLX players as cheaters
/*	if(CollisionSide & (COL_TOP|COL_BOTTOM)) {
		y=-y;
	}
	if(CollisionSide & (COL_LEFT|COL_RIGHT)) {
		x=-x;
	}

	if(CollisionSide & COL_TOP) {
		y*=fCoeff;
	}
	if(CollisionSide & COL_BOTTOM) {
		y*=fCoeff;
	}
	if(CollisionSide & COL_LEFT) {
		x*=fCoeff;
	}
	if(CollisionSide & COL_RIGHT) {
		x*=fCoeff;
	}*/


	// WARNING: this code should not be used; it is simply wrong
	//	(this was the way the original LX did it)

	if (CollisionSide & COL_TOP)  {
		x = fCoeff;
		y = -fCoeff;
	}
	if (CollisionSide & COL_BOTTOM)  {
		x = fCoeff;
		y = -fCoeff;
	}
	if (CollisionSide & COL_LEFT)  {
		x = -fCoeff;
		y = fCoeff;
	}
	if (CollisionSide & COL_RIGHT)  {
		x = -fCoeff;
		y = fCoeff;
	}


	vVelocity.x *= x;
	vVelocity.y *= y;
}


///////////////////
// Check for collisions with worms
// HINT: this function is not used at the moment
//		(ProjWormColl is used directly from within CheckCollision)
int CProjectile::CheckWormCollision(CWorm *worms)
{
	static const float divisions = 5;
	CVec dir = vPosition - vOldPos;
	float length = NormalizeVector(&dir);

	// Length must be at least 'divisions' in size so we do at least 1 check
	// So stationary projectiles also get checked (mines)
	length = MAX(length, divisions);

	// Go through at fixed positions
	CVec pos = vOldPos;
	int wrm;
	for(float p=0; p<length; p+=divisions, pos += dir*divisions) {
		wrm = ProjWormColl(pos, worms);
		if( wrm >= 0)
			return wrm;
	}

	// AI hack (i know it's dirty, but it's fast)
	// Checks, whether this projectile is heading to any AI worm
	// If so, sets the worm's property Heading to ourself
	CVec mutual_speed;
	CWorm *w = worms;
	for(short i=0;i<MAX_WORMS;i++,w++) {

		// Only AI worms need this
		if (!w->isUsed() || !w->getAlive() || w->getType() != PRF_COMPUTER)
			continue;

		mutual_speed = vVelocity - (*w->getVelocity());

		// This projectile is heading to the worm
		if (SIGN(mutual_speed.x) == SIGN(w->getPos().x - vPosition.x) &&
			SIGN(mutual_speed.y) == SIGN(w->getPos().y - vPosition.y))  {

			int len = (int)mutual_speed.GetLength();
			float dist = 60.0f;

			// Get the dangerous distance for various speeds
			if (len < 30)
				dist = 20.0f;
			else if (len < 60)
				dist = 30.0f;
			else if (len < 90)
				dist = 50.0f;

			float real_dist2 = (vPosition - w->getPos()).GetLength2();
			if (real_dist2 <= dist*dist) {
				// If any projectile is already heading to the worm and is closer than we, don't set us as heading
				if (w->getHeading())  {
					if ((w->getPos() - w->getHeading()->GetPosition()).GetLength2() >= dist*dist)
						w->setHeading(this);
				}
				// No projectile heading to the worm
				else
					w->setHeading(this);
			}

		}
	}


	// No worms hit
	return -1;
}


///////////////////
// Lower level projectile-worm collision test
int CProjectile::ProjWormColl(CVec pos, CWorm *worms)
{
	int px = (int)pos.x;
	int py = (int)pos.y;
	int wx,wy;

	const static int wsize = 4;

	CWorm *w = worms;
	for(short i=0;i<MAX_WORMS;i++,w++) {
		if(!w->isUsed() || !w->getAlive())
			continue;
		if(w->getFlag())
			continue;

		wx = (int)w->getPos().x;
		wy = (int)w->getPos().y;

		// AABB - Point test
		if( abs(wx-px) < wsize && abs(wy-py) < wsize) {

			CollisionSide = 0;

			// Calculate the side of the collision (obsolete??)
			if(px < wx-2)
				CollisionSide |= COL_LEFT;
			else if(px > wx+2)
				CollisionSide |= COL_RIGHT;
			if(py < wy-2)
				CollisionSide |= COL_TOP;
			else if(py > wy+2)
				CollisionSide |= COL_BOTTOM;

			return i;
		}
	}

	// No worm was hit
	return -1;
}
