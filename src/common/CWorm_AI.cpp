/////////////////////////////////////////
//
//             Liero Xtreme
//
//     Copyright Auxiliary Software 2002
//
//
/////////////////////////////////////////


// Worm class - AI
// Created 13/3/03
// Jason Boettcher


#include "defs.h"
#include "LieroX.h"



/*
===============================

    Artificial Intelligence

===============================
*/


///////////////////
// Initialize the AI
bool CWorm::AI_Initialize(CMap *pcMap)
{
    assert(pcMap);

    // Because this can be called multiple times, shutdown any previous allocated data
    AI_Shutdown();

    // Allocate the Open/Close grid
    nGridCols = pcMap->getGridCols();
    nGridRows = pcMap->getGridRows();

    pnOpenCloseGrid = new int[nGridCols*nGridRows];
    if(!pnOpenCloseGrid)
        return false;

    psPath = NULL;
    psCurrentNode = NULL;
    fLastCarve = -9999;
    cStuckPos = CVec(-999,-999);
    fStuckTime = -9999;
    fLastPathUpdate = -9999;
    bStuck = false;
	//iAiGameType = GAM_OTHER;

    return true;
}


///////////////////
// Shutdown the AI stuff
void CWorm::AI_Shutdown(void)
{
    AI_CleanupPath(psPath);
    psPath = NULL;
    psCurrentNode = NULL;
    if(pnOpenCloseGrid)
        delete[] pnOpenCloseGrid;
}




/*
  Algorithm:
  ----------

  1) Find nearest worm and set it as a target
  2) If we are within a good distance, aim and shoot the target
  3) If we are too far, try and get closer by digging and ninja rope

*/


///////////////////
// Simulate the AI
void CWorm::AI_GetInput(int gametype, int teamgame, int taggame, CMap *pcMap)
{
	// Behave like humans and don't play immediatelly after spawn
	if ((tLX->fCurTime-fSpawnTime) < 0.4)
		return;

	worm_state_t *ws = &tState;
	gs_worm_t *wd = cGameScript->getWorm();

	// Init the ws
	ws->iCarve = false;
	ws->iMove = false;
	ws->iShoot = false;
	ws->iJump = false;

	iAiGame = gametype;
	iAiTeams = teamgame;
	iAiTag = taggame;

    strcpy(tLX->debug_string, "");

	int DiffLevel = tProfile->nDifficulty;
	float dt = tLX->fDeltaTime;

    // Every 3 seconds we run the think function
    if(tLX->fCurTime - fLastThink > 3 && nAIState != AI_THINK)
        nAIState = AI_THINK;


    // If we have a good shooting 'solution', shoot
    if(AI_CanShoot(pcMap, gametype)) {

        // Shoot
        AI_Shoot(pcMap);
        return;
    }

	// Reload weapons when we can't shoot
	AI_ReloadWeapons();

    // Increase the last target change time
	fTargetTime += tLX->fDeltaTime;

    // Process depending on our current state
    switch(nAIState) {

        // Think; We spawn in this state
        case AI_THINK:
            AI_Think(gametype, teamgame, taggame, pcMap);
            break;

        // Moving towards a target
        case AI_MOVINGTOTARGET:
            AI_MoveToTarget(pcMap);
            break;
    }


    //
    // Find a target worm
    //
    /*CWorm *trg = findTarget(gametype, teamgame, taggame, pcMap);

	// If we have no target, do something
    if(!trg)
        // TODO Something
        return;


    //
    // Shoot the target
    //

    // If we can shoot the target, shoot it
    if(AI_CanShoot(trg, pcMap)) {

        // Shoot the target

    } else {

        // If we cannot shoot the target, walk towards the target
        AI_MoveToTarget(trg, pcMap);
    }


	//
	// Shoot the target
	//


	CVec	tgPos = trg->getPos();
	CVec	tgDir = tgPos - vPos;    

	int		iAngleToBig = false;

	float   fDistance = NormalizeVector(&tgDir);
	
	// Make me face the target
	if(tgPos.GetX() > vPos.GetX())
		iDirection = DIR_RIGHT;
	else
		iDirection = DIR_LEFT;


	// Aim at the target
	float ang = (float)atan2(tgDir.GetX(), tgDir.GetY());
	ang = RAD2DEG(ang);

	if(iDirection == DIR_LEFT)
		ang+=90;
	else
		ang = -ang + 90;

	// Clamp the angle
	ang = MAX(-90, ang);

	if(ang > 60) {
		ang = 60;
		iAngleToBig = true;
	}


	// AI Difficulty level effects accuracy
	float Levels[] = {12, 6, 3, 0};

	// If we are going to shoot, we need to lower the accuracy a little bit
	ang += GetRandomNum() * Levels[DiffLevel];


	// Move the angle at the same speed humans are allowed to move the angle
	if(ang > fAngle)
		fAngle += wd->AngleSpeed * dt;
	else if(ang < fAngle)
		fAngle -= wd->AngleSpeed * dt;

	// If the angle is within +/- 3 degrees, just snap it
	if( fabs(fAngle - ang) < 3)
		fAngle = ang;

	// Clamp the angle
	fAngle = MIN(60,fAngle);
	fAngle = MAX(-90,fAngle);


	// ???: If the angle is to big, (ie, the target is below us and we can't aim directly at him) don't shoot


	// If we are close enough, shoot the target
	if(fDistance < 200) {

        // Find the best weapon for the job
        int wpn = getBestWeapon( gametype, fDistance, tgPos, pcMap );
        if( wpn >= 0 ) {
            iCurrentWeapon = wpn;
            ws->iShoot = true;
        }
    } else {

        // If we're outside shooting range, cycle through the weapons so they can be reloaded
        iCurrentWeapon = cycleWeapons();
    }


	// If we are real close, jump to get a better angle
	// Only do this if we are better then easy level
	if(fDistance < 10 && DiffLevel > AI_EASY)
		ws->iJump = true;



	// We need to move closer to the target

	// We carve every few milliseconds so we don't go too fast
	if(tLX->fCurTime - fLastCarve > 0.35f) {
		fLastCarve = tLX->fCurTime;
		ws->iCarve = true;
	}

	ws->iMove = true;



	// If the target is above us, we point up & use the ninja rope
	if(fAngle < -60 && (vPos - tgPos).GetY() > 50) {
		
		fAngle = -90;
        CVec dir;
        dir.SetX( (float)cos(fAngle * (PI/180)) );
	    dir.SetY( (float)sin(fAngle * (PI/180)) );
	    if(iDirection==DIR_LEFT)
		    dir.SetX(-dir.GetX());

        if( !cNinjaRope.isReleased() )
            cNinjaRope.Shoot(vPos,dir);
	}


    // If the hook of the ninja rope is below us, release it
    if( cNinjaRope.isReleased() && cNinjaRope.isAttached() && cNinjaRope.getHookPos().GetY() > vPos.GetY() ) {
        cNinjaRope.Release();
    }

    // If the target is not too far above us, we should release the ninja rope
    if( (vPos - tgPos).GetY() < 30 && cNinjaRope.isAttached() && cNinjaRope.isReleased() ) {
        cNinjaRope.Release();
    }*/
}


///////////////////
// Find a target worm
CWorm *CWorm::findTarget(int gametype, int teamgame, int taggame, CMap *pcMap)
{
	CWorm	*w = cClient->getRemoteWorms();
	CWorm	*trg = NULL;
	CWorm	*nonsight_trg = NULL;
	float	fDistance = 99999;
	float	fSightDistance = 99999;

	int NumTeams = 0;
	int i;
	for (i=0; i<4; i++)
		if (cClient->getTeamScore(i) > -1)
			NumTeams++;


    //
	// Just find the closest worm
	//

	for(i=0; i<MAX_WORMS; i++, w++) {

		// Don't bother about unused or dead worms
		if(!w->isUsed() || !w->getAlive())
			continue;

		// Make sure i don't target myself
		if(w->getID() == iID)
			continue;

		// If this is a team game, don't target my team mates
		// BUT, if there is only one team, play it like deathmatch
		if(teamgame && w->getTeam() == iTeam && NumTeams > 1)
			continue;

		// If this is a game of tag, only target the worm it (unless it's me)
		if(taggame && !w->getTagIT() && !iTagIT)
			continue;

		// Don't choose the same worm, if we can't
		if(w->getID() == iLastTargetID && iForceTargetChange)
			continue;		

		// Calculate distance between us two
		float l = CalculateDistance(w->getPos(), vPos);

		// Prefer targets we have free line of sight to
		float length;
		int type;
		traceLine(w->getPos(),pcMap,&length,&type,1);
		if (type != PX_ROCK)  {
			// Line of sight not blocked
			if (l < fSightDistance)  {
				trg = w;
				fSightDistance = l;
				if (l < fDistance)  {
					nonsight_trg = w;
					fDistance = l;
				}
			}
		}
		else
			// Line of sight blocked
			if(l < fDistance) {
				nonsight_trg = w;
				fDistance = l;
			}
	}

	// If the target we have line of sight to is too far, switch back to the closest target
	if ((fSightDistance-fDistance > 50.0f) || trg == NULL)  {
		if (nonsight_trg)
			trg = nonsight_trg;
	}

	// Reset the target time
	fTargetTime = 0;

	// Don't force target change anymore
	iForceTargetChange = false;

	if (trg)  {
		if (trg->getID() != iLastTargetID)
			fLastShoot = 0;
		iLastTargetID = trg->getID();
	}

    return trg;
}


///////////////////
// Think State
void CWorm::AI_Think(int gametype, int teamgame, int taggame, CMap *pcMap)
{
    /*
      We start of in an think state. When we're here we decide what we should do.
      If there is an unfriendly worm, or a game target we deal with it.
      In the event that we have no unfriendly worms or game targets we should remain in the idle state.
      While idling we can walk around to appear non-static, reload weapons or grab a bonus.
    */

    // Clear the state
    psAITarget = NULL;
    psBonusTarget = NULL;
    nAITargetType = AIT_NONE;
    fLastThink = tLX->fCurTime;


    // Reload our weapons in idle mode
    AI_ReloadWeapons();


    // If our health is less than 15% (critical), our main priority is health
    if(iHealth < 15)
        if(AI_FindHealth(pcMap))
            return;

    // Search for an unfriendly worm
    psAITarget = findTarget(gametype, teamgame, taggame, pcMap);

	/*bool bGoodTarget = true;
	// Not a good target probably
	if ((tLX->fCurTime-fLastShoot) > 10 && fLastShoot)
		bGoodTarget = false;*/

    
    // Any unfriendlies?
    if(psAITarget/* && bGoodTarget*/) {
        // We have an unfriendly target, so change to a 'move-to-target' state
        nAITargetType = AIT_WORM;
        nAIState = AI_MOVINGTOTARGET;
        AI_InitMoveToTarget(pcMap);
        return;
    }

	if(!psAITarget)
		fLastShoot = 0;


    // If we're down on health (less than 80%) we should look for a health bonus
    if(iHealth < 80) {
        if(AI_FindHealth(pcMap))
            return;
    }


    /*
      If we get here that means we have nothing to do
    */


    //
    // Typically, high ground is safer. So if we don't have anything to do, lets up up
    //
    
    // Our target already on high ground?
    if(cPosTarget.GetY() < pcMap->getGridHeight()*5 && nAIState == AI_MOVINGTOTARGET)  {

		// Move a bit
		if (IsEmpty(CELL_LEFT,pcMap))  {
			iDirection = DIR_LEFT;
			cPosTarget.SetX(vPos.GetX()+(pcMap->getGridWidth()/pcMap->getGridCols()));
			return;
		}

		if (IsEmpty(CELL_UP,pcMap))  {
			cPosTarget.SetY(vPos.GetY()+(pcMap->getGridHeight()/pcMap->getGridRows()));
			return;
		}

		if (IsEmpty(CELL_RIGHT,pcMap))  {
			iDirection = DIR_RIGHT;
			cPosTarget.SetX(vPos.GetX()-(pcMap->getGridWidth()/pcMap->getGridCols()));
			return; 
		}


		// Nothing todo, so go find some health if we even slightly need it
		if(iHealth < 100) {
			if(AI_FindHealth(pcMap))
				return;
		}
        return;
	}

    // Find a random spot to go to high in the level
    int x, y;
    for(y=1; y<5 && y<pcMap->getGridRows()-1; y++) {
        for(x=1; x<pcMap->getGridCols()-1; x++) {
            uchar pf = *(pcMap->getGridFlags() + y*pcMap->getGridCols() + x);

            if(pf & PX_ROCK)
                continue;

            // Set the target
            cPosTarget = CVec((float)(x*pcMap->getGridWidth()+(pcMap->getGridWidth()/2)), (float)(y*pcMap->getGridHeight()+(pcMap->getGridHeight()/2)));
            nAITargetType = AIT_POSITION;
            nAIState = AI_MOVINGTOTARGET;
            AI_InitMoveToTarget(pcMap);
            break;
        }
    }

    /*int     x, y;
    int     px, py;
    bool    first = true;
    int     cols = pcMap->getGridCols()-1;       // Note: -1 because the grid is slightly larger than the
    int     rows = pcMap->getGridRows()-1;       // level size
    int     gw = pcMap->getGridWidth();
    int     gh = pcMap->getGridHeight();
	CVec	resultPos;

    
    // Find a random cell to start in
    px = (int)(fabs(GetRandomNum()) * (float)cols);
	py = (int)(fabs(GetRandomNum()) * (float)rows);

    x = px; y = py;
	bool breakme = false;

    // Start from the cell and go through until we get to an empty cell
    while(1) {
        while(1) {
            // If we're on the original starting cell, and it's not the first move we have checked all cells
            // and should leave
            if(!first) {
                if(px == x && py == y) {
                    resultPos = CVec((float)x*gw+gw/2, (float)y*gh+gh/2);
					breakme = true;
					break;
                }
            }
            first = false;

            uchar pf = *(pcMap->getGridFlags() + y*pcMap->getGridCols() + x);
            if(pf != PX_ROCK)  {
                resultPos = CVec((float)x*gw+gw/2, (float)y*gh+gh/2);
				breakme = true;
				break;
			}
            
            if(++x >= cols) {
                x=0;
                break;
            }
        }

		if (breakme)
			break;

        if(++y >= rows) {
            y=0;
            x=0;
        }
    }


   //cPosTarget = resultPos;
   nAITargetType = AIT_POSITION;
   nAIState = AI_MOVINGTOTARGET;
   AI_InitMoveToTarget(pcMap);*/


}


///////////////////
// Find a health pack
// Returns true if we found one
bool CWorm::AI_FindHealth(CMap *pcMap)
{
	if (!tGameInfo.iBonusesOn)
		return false;

    CBonus  *pcBonusList = cClient->getBonusList();
    int     i;
    bool    bFound = false;
    CBonus  *pcBonus = NULL;
    float   dist = 99999;

    // Find the closest health bonus
    for(i=0; i<MAX_BONUSES; i++) {
        if(pcBonusList[i].getUsed() && pcBonusList[i].getType() == BNS_HEALTH) {
            
            float d = CalculateDistance(pcBonusList[i].getPosition(), vPos);

            if(d < dist) {
                pcBonus = &pcBonusList[i];
                dist = d;
                bFound = true;
            }
        }
    }


    // TODO: Verify that the target is not in some small cavern that is hard to get to

    
    // If we have found a bonus, setup the state to move towards it
    if(bFound) {
        psBonusTarget = pcBonus;
        nAITargetType = AIT_BONUS;
        nAIState = AI_MOVINGTOTARGET;
        AI_InitMoveToTarget(pcMap);
    }

    return bFound;
}


///////////////////
// Reloads the weapons
void CWorm::AI_ReloadWeapons(void)
{
    int     i;

    // Go through reloading the weapons
    for(i=0; i<5; i++) {
        if(tWeapons[i].Reloading) {
            iCurrentWeapon = i;
            break;
        }
    }
}


///////////////////
// Initialize a move-to-target
void CWorm::AI_InitMoveToTarget(CMap *pcMap)
{
    memset(pnOpenCloseGrid, 0, nGridCols*nGridRows*sizeof(int));

    // Cleanup the old path
    AI_CleanupPath(psPath);

    cPosTarget = AI_GetTargetPos();

    // Put the target into a cell position
    int tarX = (int) cPosTarget.GetX() / pcMap->getGridWidth();
    int tarY = (int) cPosTarget.GetY() / pcMap->getGridHeight();

    // Current position cell
    int curX = (int) vPos.GetX() / pcMap->getGridWidth();
    int curY = (int) vPos.GetY() / pcMap->getGridHeight();

    nPathStart[0] = curX;
    nPathStart[1] = curY;
    nPathEnd[0] = tarX;
    nPathEnd[1] = tarY;
   
    // Go down the path
    psPath = AI_ProcessNode(pcMap, NULL, curX,curY, tarX, tarY);
    psCurrentNode = psPath;

    fLastPathUpdate = tLX->fCurTime;
    
    // Draw the path
    if(!psPath)
        return;

#ifdef _AI_DEBUG
    pcMap->DEBUG_DrawPixelFlags();
    AI_DEBUG_DrawPath(pcMap,psPath);
#endif // _AI_DEBUG

}


///////////////////
// Path finding 
ai_node_t *CWorm::AI_ProcessNode(CMap *pcMap, ai_node_t *psParent, int curX, int curY, int tarX, int tarY)
{
    int     i;
    bool    bFound = false;

    // Bounds checking
    if(curX < 0 || curY < 0 || tarX < 0 || tarY < 0)
        return NULL;
    if(curX >= nGridCols || curY >= nGridRows || tarX >= nGridCols || tarY >= nGridRows)
        return NULL;

    // Are we on target?
    if(curX == tarX && curY == tarY)
        bFound = true;

    // Is this node impassable (rock)?
    uchar pf = *(pcMap->getGridFlags() + (curY*nGridCols) + curX);
    if(pf == PX_ROCK && !bFound) {
        if(psParent != NULL)  // Parent can be on rock
            return NULL;
    }

    int movecost = 1;
    // Dirt is a higher cost
    if(pf == PX_DIRT)
        movecost = 2;

    // Is this node closed?    
    if(pnOpenCloseGrid[curY*nGridCols+curX] == 1)
        return NULL;
    
    // Close it
    pnOpenCloseGrid[curY*nGridCols+curX] = 1;

    // Create a node
    ai_node_t *node = new ai_node_t;
    if(!node)
        return NULL;

    // Fill in the node details
    node->psParent = psParent;
    node->nX = curX;
    node->nY = curY;
    node->nCost = 0;
    node->nCount = 0;
    node->nFound = false;
    node->psPath = NULL;
    for(i=0; i<8; i++)
        node->psChildren[i] = NULL;
    if(psParent) {
        node->nCost = psParent->nCost+movecost;
        node->nCount = psParent->nCount+1;
    }

    // Are we on target?
    if(bFound) {
		tState.iJump = true;
        node->nFound = true;
        // Traverse back up the path
        return node;
    }

    /*DrawRectFill(pcMap->GetImage(), curX*pcMap->getGridWidth(), curY*pcMap->getGridHeight(), 
                                    curX*pcMap->getGridWidth()+pcMap->getGridWidth(),
                                    curY*pcMap->getGridHeight()+pcMap->getGridHeight(), MakeColour(0,0,255));*/


    //
    // TEST
    //

    int closest = -1;
    int cost = 99999;
    /*int norm[] = {-1,-1, 0,-1, 1,-1, 
                  -1,0,  1,0,
                  -1,1,  0,1, 1,1};
    for(i=0; i<8; i++) {
        node->psChildren[i] = AI_ProcessNode(pcMap, node, curX+norm[i*2], curY+norm[i*2+1], tarX, tarY);
        if(node->psChildren[i]) {
            if(node->psChildren[i]->nFound) {
                if(node->psChildren[i]->nCost < cost) {
                    cost = node->psChildren[i]->nCost;
                    closest = i;
                }
            }
        }
    }
    
    // Kill the children
    for(i=0; i<8; i++) {
        if(i != closest) {
            delete node->psChildren[i];
            node->psChildren[i] = NULL;
        }
    }
    
    // Found any path?
    if(closest == -1) {
        delete node;
        return NULL;
    }
    
    // Set the closest path
    node->psPath = node->psChildren[closest];
    node->nFound = true;
    return node;*/



    // Here we go down the children that are closest to the target
    int vertDir = 0;    // Middle
    int horDir = 0;     // Middle

    if(tarY < curY)
        vertDir = -1;   // Go upwards
    else if(tarY > curY)
        vertDir = 1;    // Go downwards
    if(tarX < curX)
        horDir = -1;    // Go left
    else if(tarX > curX)
        horDir = 1;     // Go right


    int diagCase[] = {horDir,vertDir,  horDir,0,  0,vertDir,  -horDir,vertDir,
                      horDir,-vertDir, -horDir,0, 0,-vertDir, -horDir,-vertDir};

    //
    // Diagonal target case
    //
    closest = -1;
    cost = 99999;
    if(horDir != 0 && vertDir != 0) {
        for(i=0; i<8; i++) {
            node->psChildren[i] = AI_ProcessNode(pcMap, node, curX+diagCase[i*2], curY+diagCase[i*2+1], tarX, tarY);
            if(node->psChildren[i]) {
                if(node->psChildren[i]->nFound) {
                    if(node->psChildren[i]->nCost < cost) {
                        cost = node->psChildren[i]->nCost;
                        closest = i;
                    }
                }
            }
        }

        // Kill the children
        for(i=0; i<8; i++) {
            if(i != closest) {
				if(node->psChildren[i]) {
					delete node->psChildren[i];
					node->psChildren[i] = NULL;
				}
            }
        }

        // Found any path?
        if(closest == -1) {
			if(node)
				delete node;
            return NULL;
        }

        // Set the closest path
        node->psPath = node->psChildren[closest];
        node->nFound = true;
        return node;
    }


    //
    // Straight target case
    //
    int upCase[] = {0,-1,  1,-1,  -1,-1,  1,0,  -1,0,  1,1,  -1,1,  0,1};
    int rtCase[] = {1,0,   1,1,   1,-1,   0,1,  0,-1,  -1,1, -1,-1, -1,0};
    
    closest = -1;
    cost = 99999;
    for(i=0; i<8; i++) {
        int h,v;
        // Up/Down
        if(horDir == 0) {
            h = upCase[i*2];
            v = upCase[i*2+1];
            if(vertDir == 1) {
                h = -h;
                v = -v;
            }
        }

        // Left/Right
        if(vertDir == 0) {
            h = rtCase[i*2];
            v = rtCase[i*2+1];
            if(horDir == -1) {
                h = -h;
                v = -v;
            }
        }

        node->psChildren[i] = AI_ProcessNode(pcMap, node, curX+h, curY+v, tarX, tarY);
        if(node->psChildren[i]) {
            if(node->psChildren[i]->nFound) {
                if(node->psChildren[i]->nCost < cost) {
                    cost = node->psChildren[i]->nCost;
                    closest = i;
                }
            }
        }
        
        // Kill the children
        for(i=0; i<8; i++) {
            if(i != closest) {
				if(node->psChildren[i]) {
					delete node->psChildren[i];
					node->psChildren[i] = NULL;
				}
            }
        }
        
        // Found any path?
        if(closest == -1) {
			if(node)
				delete node;
            return NULL;
        }
        
        // Set the closest path
        node->psPath = node->psChildren[closest];
        node->nFound = true;
        return node;
    }
    delete node;
    return NULL;
}


///////////////////
// Cleanup a path
void CWorm::AI_CleanupPath(ai_node_t *node)
{
    if(node) {

        AI_CleanupPath(node->psPath);
        delete node;
    }
}


///////////////////
// Move towards a target
void CWorm::AI_MoveToTarget(CMap *pcMap)
{
    int     nCurrentCell[2];
    int     nTargetCell[2];
    int     nFinalTarget[2];
    worm_state_t *ws = &tState;

    int     hgw = pcMap->getGridWidth()/2;
    int     hgh = pcMap->getGridHeight()/2;

	// Better target?
	CWorm *newtrg = findTarget(iAiGame, iAiTeams, iAiTag, pcMap);
	if (psAITarget && newtrg)
		if (newtrg->getID() != psAITarget->getID())
			nAIState = AI_THINK;

    // Clear the state
	ws->iCarve = false;
	ws->iMove = false;
	ws->iShoot = false;
	ws->iJump = false;

    cPosTarget = AI_GetTargetPos();

    // If we're really close to the target, perform a more precise type of movement
    if(fabs(vPos.GetX() - cPosTarget.GetX()) < 20 && fabs(vPos.GetY() - cPosTarget.GetY()) < 20) {
        AI_PreciseMove(pcMap);
        return;
    }

    // If we're stuck, just get out of wherever we are
    if(bStuck) {
        ws->iMove = true;
        ws->iJump = true;
        cNinjaRope.Release();
        if(tLX->fCurTime - fStuckPause > 2)
            bStuck = false;
        return;
    }



    /*
      First, if the target or ourself has deviated from the path target & start by a large enough amount:
      Or, enough time has passed since the last path update:
      recalculate the path
    */
    int     i;
    int     nDeviation = 2;     // Maximum deviation allowance
    bool    recalculate = false;

    
    // Cell positions
    nTargetCell[0] = (int)cPosTarget.GetX() / pcMap->getGridWidth();
    nTargetCell[1] = (int)cPosTarget.GetY() / pcMap->getGridHeight();
    nCurrentCell[0] = (int)vPos.GetX() / pcMap->getGridWidth();
    nCurrentCell[1] = (int)vPos.GetY() / pcMap->getGridHeight();
    nFinalTarget[0] = (int)cPosTarget.GetX() / pcMap->getGridWidth();
    nFinalTarget[1] = (int)cPosTarget.GetY() / pcMap->getGridHeight();

    for(i=0 ;i<2; i++) {
        if(abs(nPathStart[i] - nCurrentCell[i]) > nDeviation ||
            abs(nPathEnd[i] - nTargetCell[i]) > nDeviation) {
            recalculate = true;
        }
    }

    if(tLX->fCurTime - fLastPathUpdate > 3)
        recalculate = true;

    // Re-calculate the path?
    if(recalculate)
        AI_InitMoveToTarget(pcMap);

    
    // We carve every few milliseconds so we don't go too fast
	if(tLX->fCurTime - fLastCarve > 0.35f) {
		fLastCarve = tLX->fCurTime;
		ws->iCarve = true;
	}


    /*
      Move through the path.
      We have a current node that we must get to. If we go onto the node, we go to the next node, and so on.
      Because the path finding isn't perfect we can sometimes skip a whole group of nodes.
      If we are close to other nodes that are _ahead_ in the list, we can skip straight to that node.
    */

    int     nNodeProx = 3;      // Maximum proximity to a node to skip to

    if(psCurrentNode == NULL || psPath == NULL) {
        #ifdef _AI_DEBUG
          pcMap->DEBUG_DrawPixelFlags();
          AI_DEBUG_DrawPath(pcMap, psPath);
		#endif // _AI_DEBUG

        // If we don't have a path, resort to simpler AI methods
        AI_SimpleMove(pcMap,psAITarget != NULL);
        return;
    }

    float xd = (float)(nCurrentCell[0] - psCurrentNode->nX);
    float yd = (float)(nCurrentCell[1] - psCurrentNode->nY);
    float fCurDist = fastSQRT(xd*xd + yd*yd);
    
    // Go through the path
    ai_node_t *node = psPath;    
    for(; node; node=node->psPath) {

        // Don't go back down the list
        if(node->nCount <= psCurrentNode->nCount)
            continue;

         // If the node is blocked, don't skip ahead
        float tdist;
        int type;
        traceLine(CVec((float)(node->nX*pcMap->getGridWidth()+hgw), (float) (node->nY*pcMap->getGridHeight()+hgh)), pcMap, &tdist, &type,1);
        if(tdist < 0.75f && type == PX_ROCK)
            continue;

        // If we are near the node skip ahead to it
        if(abs(node->nX - nCurrentCell[0]) < nNodeProx &&
           abs(node->nY - nCurrentCell[1]) < nNodeProx) {

            psCurrentNode = node;

			#ifdef _AI_DEBUG
				pcMap->DEBUG_DrawPixelFlags();
				AI_DEBUG_DrawPath(pcMap, node);
			#endif  // _AI_DEBUG
            continue;
        }

        // If we're closer to this node than to our current node, skip to this node
        xd = (float)(nCurrentCell[0] - node->nX);
        yd = (float)(nCurrentCell[1] - node->nY);
        float dist = fastSQRT(xd*xd + yd*yd);

        if(dist < fCurDist) {
            psCurrentNode = node;
			#ifdef _AI_DEBUG
				pcMap->DEBUG_DrawPixelFlags();
				AI_DEBUG_DrawPath(pcMap, node);
			#endif  // _AI_DEBUG
        }
    }

    if(psCurrentNode == NULL || psPath == NULL) {
         // If we don't have a path, resort to simpler AI methods
        AI_SimpleMove(pcMap);
        return;
    }

    nTargetCell[0] = psCurrentNode->nX;
    nTargetCell[1] = psCurrentNode->nY;


    /*
      We get the 5 next nodes in the list and generate an average position to get to
    */
    int avgCell[2] = {0,0};
    node = psCurrentNode;    
    for(i=0; node && i<5; node=node->psPath, i++) {
        avgCell[0] += node->nX;
        avgCell[1] += node->nY;
    }
    avgCell[0] /= i;
    avgCell[1] /= i;
    //nTargetCell[0] = avgCell[0];
    //nTargetCell[1] = avgCell[1];
    
    tLX->debug_pos = CVec((float)(nTargetCell[0]*pcMap->getGridWidth()+hgw), (float)(nTargetCell[1]*pcMap->getGridHeight()+hgh));



    /*
      Now that we've done all the boring stuff, our single job here is to reach the node.
      We have walking, jumping, move-through-air, and a ninja rope to help us.
    */
    int     nRopeHeight = 3;                // Minimum distance to release rope    


    // Aim at the node
    bool aim = AI_SetAim(CVec((float)(nTargetCell[0]*pcMap->getGridWidth()+hgw), (float)(nTargetCell[1]*pcMap->getGridHeight()+hgh)));

    // If we are stuck in the same position for a while, take measures to get out of being stuck
    if(fabs(cStuckPos.GetX() - vPos.GetX()) < 25 && fabs(cStuckPos.GetY() - vPos.GetY()) < 25) {
        fStuckTime += tLX->fDeltaTime;

        // Have we been stuck for a few seconds?
        if(fStuckTime > 4/* && !ws->iJump && !ws->iMove*/) {
            // Jump, move, switch directions and release the ninja rope
            ws->iJump = true;
            ws->iMove = true;
            bStuck = true;
            fStuckPause = tLX->fCurTime;
            cNinjaRope.Release();

			iDirection = !iDirection;
            
            fAngle -= 45;
            // Clamp the angle
	        fAngle = MIN(60,fAngle);
	        fAngle = MAX(-90,fAngle);

            // Recalculate the path
            AI_InitMoveToTarget(pcMap);
            fStuckTime = 0;
        }
    }
    else {
        bStuck = false;
        fStuckTime = 0;
        cStuckPos = vPos;
    }


    // If the ninja rope hook is falling, release it & walk
    if(!cNinjaRope.isAttached() && !cNinjaRope.isShooting()) {
        cNinjaRope.Release();
        ws->iMove = true;
    }

    // Walk only if the target is a good distance on either side
    if(abs(nCurrentCell[0] - nTargetCell[0]) > 3)
        ws->iMove = true;

    
    // If the node is above us by a lot, we should use the ninja rope
    if(nTargetCell[1] < nCurrentCell[1]) {
        
        // If we're aimed at the point, just leave and it'll happen soon
        if(!aim)
            return;

        bool fireNinja = true;

        CVec dir;
        dir.SetX( (float)cos(fAngle * (PI/180)) );
	    dir.SetY( (float)sin(fAngle * (PI/180)) );
	    if(iDirection == DIR_LEFT)
		    dir.SetX(-dir.GetX());
       
        /*
          Got aim, so shoot a ninja rope
          We shoot a ninja rope if it isn't shot
          Or if it is, we make sure it has pulled us up and that it is attached
        */
        if(fireNinja) {
            if(!cNinjaRope.isReleased())
                cNinjaRope.Shoot(vPos,dir);
            else {
                float length = CalculateDistance(vPos, cNinjaRope.getHookPos());
                if(cNinjaRope.isAttached()) {
                    if(length < cNinjaRope.getRestLength() && vVelocity.GetY()<-10)
                        cNinjaRope.Shoot(vPos,dir);
                }
            }
        }
    }    


    /*
      If there is dirt between us and the next node, don't shoot a ninja rope
      Instead, carve or use some clearing weapon
    */
    float traceDist = -1;
    int type = 0;
	if (!psCurrentNode)
		return;
    CVec v = CVec((float)(psCurrentNode->nX*pcMap->getGridWidth()+hgw), (float)(psCurrentNode->nY*pcMap->getGridHeight()+hgh));
    int length = traceLine(v, pcMap, &traceDist, &type);
    float dist = CalculateDistance(v, vPos);
    if(length < dist && type == PX_DIRT) {
        ws->iJump = true;
        ws->iMove = true;
		ws->iCarve = true; // Carve
        
        // Shoot a path
        /*int wpn;
        if((wpn = AI_FindClearingWeapon()) != -1) {
            iCurrentWeapon = wpn;
            ws->iShoot = true;
            cNinjaRope.Release();
        }*/
    }



    // If we're above the node, let go of the rope and move towards to node
    if(nTargetCell[1] >= nCurrentCell[1]+nRopeHeight) {
        // Let go of any rope
        cNinjaRope.Release();

        // Walk in the direction of the node
        ws->iMove = true;
    }

    if(nTargetCell[1] >= nCurrentCell[1]) {

		if(!IsEmpty(CELL_DOWN,pcMap))  {
			if(IsEmpty(CELL_LEFT,pcMap))
				iDirection = DIR_LEFT;
			else if (IsEmpty(CELL_RIGHT,pcMap))
				iDirection = DIR_RIGHT;
		}

        // Walk in the direction of the node
        ws->iMove = true;
    }


    // If we're near the height of the final target, release the rope
    if(abs(nFinalTarget[0] - nCurrentCell[0]) < 2 && abs(nFinalTarget[1] - nCurrentCell[1]) < 2) {
        // Let go of any rope
        cNinjaRope.Release();

        // Walk in the direction of the target
        ws->iMove = true;

        // If we're under the final target, jump
        if(nFinalTarget[1] < nCurrentCell[1])
            ws->iJump = true;
    }
    





    






    

  



}


///////////////////
// DEBUG: Draw the path
void CWorm::AI_DEBUG_DrawPath(CMap *pcMap, ai_node_t *node)
{
    if(!node)
        return;

    int x = node->nX * pcMap->getGridWidth();
    int y = node->nY * pcMap->getGridHeight();
    
    //DrawRectFill(pcMap->GetImage(), x,y, x+pcMap->getGridWidth(), y+pcMap->getGridHeight(), 0);
    
    if(node->psPath) {
        int cx = node->psPath->nX * pcMap->getGridWidth();
        int cy = node->psPath->nY * pcMap->getGridHeight();
        DrawLine(pcMap->GetDrawImage(), (x+pcMap->getGridWidth()/2)*2, (y+pcMap->getGridHeight()/2)*2, 
                                    (cx+pcMap->getGridWidth()/2)*2,(cy+pcMap->getGridHeight()/2)*2,
                                    0xffff);
    }
    else {
        // Final target
        DrawRectFill(pcMap->GetDrawImage(), x*2-5,y*2-5,x*2+5,y*2+5, MakeColour(0,255,0));

    }

    AI_DEBUG_DrawPath(pcMap, node->psPath);
}


///////////////////
// Get the target's position
// Also checks the target and resets to a think state if needed
CVec CWorm::AI_GetTargetPos(void)
{
    // Put the target into a position
    switch(nAITargetType) {

        // Bonus target
        case AIT_BONUS:
            if(psBonusTarget) {
                if(!psBonusTarget->getUsed())
                    nAIState = AI_THINK;
                return psBonusTarget->getPosition();
            }
            break;

        // Worm target
        case AIT_WORM:
            if(psAITarget) {
                if(!psAITarget->getAlive() || !psAITarget->isUsed())
                    nAIState = AI_THINK;
                return psAITarget->getPos();
            }
            break;

        // Position target
        case AIT_POSITION:
            return cPosTarget;
    }

    // No target
    nAIState = AI_THINK;
    return CVec(0,0);
}


///////////////////
// Aim at a spot
// Returns true if we're aiming at it
bool CWorm::AI_SetAim(CVec cPos)
{
    float   dt = tLX->fDeltaTime;
    CVec	tgPos = cPos;
	CVec	tgDir = tgPos - vPos;
    bool    goodAim = false;
    gs_worm_t *wd = cGameScript->getWorm();

	float   fDistance = NormalizeVector(&tgDir);

	// We can't aim target straight below us
	if(tgPos.GetX()-10 < vPos.GetX() && tgPos.GetX()+10 > vPos.GetX())
		return false;
	
	if (tLX->fCurTime - fLastFace > 0.1)  {  // prevent turning
	// Make me face the target
		if(tgPos.GetX() > vPos.GetX())
			iDirection = DIR_RIGHT;
		else
			iDirection = DIR_LEFT;

		fLastFace = tLX->fCurTime;
	}

	// Aim at the target
	float ang = (float)atan2(tgDir.GetX(), tgDir.GetY());
	ang = RAD2DEG(ang);

	if(iDirection == DIR_LEFT)
		ang+=90;
	else
		ang = -ang + 90;

	// Clamp the angle
	ang = MAX(-90, ang);

	// Move the angle at the same speed humans are allowed to move the angle
	if(ang > fAngle)
		fAngle += wd->AngleSpeed * dt;
	else if(ang < fAngle)
		fAngle -= wd->AngleSpeed * dt;

	// If the angle is within +/- 3 degrees, just snap it
    if( fabs(fAngle - ang) < 3) {
		fAngle = ang;
        goodAim = true;
    }

	// Clamp the angle
	fAngle = MIN(60,fAngle);
	fAngle = MAX(-90,fAngle);

    return goodAim;
}


///////////////////
// A simpler method to get to a target
// Used if we have no path
float fLastTurn = 0;  // Time when we last tried to change the direction
float fLastJump = 0;  // Time when we last tried to jump
void CWorm::AI_SimpleMove(CMap *pcMap, bool bHaveTarget)
{
    worm_state_t *ws = &tState;

    // Simple    
	ws->iMove = true;
	ws->iShoot = false;
	ws->iJump = false;

    //strcpy(tLX->debug_string, "AI_SimpleMove invoked");

    cPosTarget = AI_GetTargetPos();
  
    // We carve every few milliseconds so we don't go too fast
	if(tLX->fCurTime - fLastCarve > 0.35f) {
		fLastCarve = tLX->fCurTime;
		ws->iCarve = true;
	}
    
    // Aim at the node
    bool aim = AI_SetAim(cPosTarget);

    // If our line is blocked, try some evasive measures
    float fDist = 0;
    int type = 0;
    int nLength = traceLine(cPosTarget, pcMap, &fDist, &type, 1);
    if(fDist < 0.75f || cPosTarget.GetY() < vPos.GetY()) {

        // Change direction
		if (bHaveTarget && (tLX->fCurTime-fLastTurn) > 1.0)  {
			iDirection = !iDirection;
			fLastTurn = tLX->fCurTime;
		}

        // Look up for a ninja throw
        aim = AI_SetAim(vPos+CVec(GetRandomNum()*10,GetRandomNum()*10+10));
        if(aim) {
            CVec dir;
            dir.SetX( (float)cos(fAngle * (PI/180)) );
	        dir.SetY( (float)sin(fAngle * (PI/180)) );
	        if(iDirection==DIR_LEFT)
		        dir.SetX(-dir.GetX());

            cNinjaRope.Shoot(vPos,dir);
        }

		// Jump and move
		else  {
			if (tLX->fCurTime-fLastJump > 3.0)  {
				ws->iJump = true;
				fLastJump = tLX->fCurTime;
			}
			ws->iMove = true;
			cNinjaRope.Release();
		}

        return;
    }

    // Release the ninja rope
    cNinjaRope.Release();
}


///////////////////
// Perform a precise movement
void CWorm::AI_PreciseMove(CMap *pcMap)
{
    worm_state_t *ws = &tState;

    //strcpy(tLX->debug_string, "AI_PreciseMove invoked");

    ws->iJump = false;
    ws->iMove = false;
    ws->iCarve = false;
    
    // If we're insanely close, just stop
    if(fabs(vPos.GetX() - cPosTarget.GetX()) < 3 && fabs(vPos.GetY() - cPosTarget.GetY()) < 3)
        return;

    // We carve every few milliseconds so we don't go too fast
	if(tLX->fCurTime - fLastCarve > 0.35f) {
		fLastCarve = tLX->fCurTime;
		ws->iCarve = true;
	}


    // Aim at the target
    //bool aim = AI_SetAim(cPosTarget);
	bool aim = AI_CanShoot(pcMap,iAiGame);
    if(aim) {
        // Walk towards the target
        ws->iMove = true;

        // If the target is above us, jump
        if(fabs(vPos.GetX() - cPosTarget.GetX()) < 5 && vPos.GetY() - cPosTarget.GetY() > 5)
            ws->iJump = true;
    } else  {
		ws->iJump = true;
		if (IsEmpty(CELL_LEFT,pcMap))
			iDirection = DIR_LEFT;
		else if (IsEmpty(CELL_RIGHT,pcMap))
			iDirection = DIR_RIGHT;
		ws->iMove = true;
	}
}


///////////////////
// Finds a suitable 'clearing' weapon
// A weapon used for making a path
// Returns -1 on failure
int CWorm::AI_FindClearingWeapon(void)
{
	if(iAiGameType == GAM_MORTARS)
		return -1;
    //if(!strcmp(tWeapons[0].Name,"Rifle"))
        for (int i=0; i<5; i++)
			if(!tWeapons[i].Reloading)
				return i;

    // No suitable weapons
    return -1;
}



///////////////////
// Can we shoot the target
bool CWorm::AI_CanShoot(CMap *pcMap, int nGameType)
{
	/*if ((tLX->fCurTime-fLastShoot) > 10 && fLastShoot)
		return true;*/

    // Make sure the target is a worm
    if(nAITargetType != AIT_WORM)
        return false;
    
    // Make sure the worm is good
    if(!psAITarget)
        return false;
    if(!psAITarget->getAlive() || !psAITarget->isUsed())
        return false;
    

    CVec    cTrgPos = psAITarget->getPos();
    bool    bDirect = true;


    /*
      Here we check if we have a line of sight with the target.
      If we do, and our 'direct' firing weapons are loaded, we shoot.

      If the line of sight is blocked, we use 'indirect' shooting methods.
      We have to be careful with indirect, because if we're in a confined space, or the target is above us
      we shouldn't shoot at the target

      The aim of the game is killing worms, so we put a higher priority on shooting rather than
      thinking tactically to prevent injury to self

	  If we play a mortar game, make sure there's enough free space to shoot - avoid suicides
    */


    // If the target is too far away we can't shoot at all (but not when a rifle game)
    float d = CalculateDistance(cTrgPos, vPos);
    if(d > 300.0f && iAiGameType != GAM_RIFLES)
        return false;

	// If we're on the target, simply shoot
	if(d < 10.0f)
		return true;

    float fDist;
    int nType = -1;
    int length = 0;

	if (iAiGameType == GAM_100LT)
		length = traceLine(cTrgPos, pcMap, &fDist, &nType,2);
	else
		length = traceLine(cTrgPos, pcMap, &fDist, &nType, 3);

    // If target is blocked by rock we can't use direct firing
    if(nType == PX_ROCK)  {
		if(iAiGameType == GAM_RIFLES || iAiGameType == GAM_MORTARS)
			return false;
        bDirect = false;
	}

	// If target is blocked by large amount of dirt, we can't shoot it with rifle
	if (iAiGameType == GAM_RIFLES && nType == PX_DIRT)  {
		if(d-fDist > 40.0f)
			return false;
	}

	// In mortar game there must be enought of free cells around us
	// BUT, if the target is really close, risk it
	if(iAiGameType == GAM_MORTARS && d > 50.0f)  {
		int EmptyCells = 0;
		if(iDirection == DIR_LEFT)  {
			EmptyCells = IsEmpty(CELL_LEFT,pcMap) + IsEmpty(CELL_CURRENT,pcMap) + IsEmpty(CELL_LEFTUP, pcMap) + IsEmpty(CELL_LEFTDOWN,pcMap) + IsEmpty(CELL_DOWN,pcMap) + IsEmpty(CELL_UP,pcMap);
			if(EmptyCells < 6)
				return false;
		}
		else  {
			EmptyCells = IsEmpty(CELL_RIGHT,pcMap) + IsEmpty(CELL_CURRENT,pcMap) + IsEmpty(CELL_RIGHTUP, pcMap) + IsEmpty(CELL_RIGHTDOWN,pcMap) + IsEmpty(CELL_DOWN,pcMap) + IsEmpty(CELL_UP,pcMap);
			if(EmptyCells < 6)
				return false;
		}

	}

    // Set the best weapon for the situation
    // If there is no good weapon, we can't shoot
    tLX->debug_float = d;
    int wpn = AI_GetBestWeapon(nGameType, d, bDirect, pcMap, fDist);
    if(wpn == -1) {
        //strcpy(tLX->debug_string, "No good weapon");
        return false;
    }     

    iCurrentWeapon = wpn;

    // Shoot
    return true;
    


    // Can't shoot for now
    return false;
}

/*bool CWorm::AI_CanShoot(CWorm *Target,CMap *Map);
{
	CVec vShootDirection;
	vShootDirection.SetX(fabs(Target->GetX()-GetX()));
	vShootDirection.SetY(fabs(Target->GetY()-GetY()));

	for (int i=0; i<VectorLength(vShootDirection); i++)
		
}*/


///////////////////
// Shoot!
void CWorm::AI_Shoot(CMap *pcMap)
{
	if(!psAITarget)
		return;
    CVec    cTrgPos = psAITarget->getPos();

    //
    // Aim at the target
    //
    bool    bAim = AI_SetAim(cTrgPos);
    if(!bAim)  {

		// In mortars we can hit the target below us
		if (iAiGameType == GAM_MORTARS)  {
			if (cTrgPos.GetY() > (vPos.GetY()-20.0f))
				tState.iShoot = true;
			return;
		}

		tState.iMove = true;
		fBadAimTime += tLX->fDeltaTime;
		if((fBadAimTime) > 4) {
			if(IsEmpty(CELL_UP,pcMap))
				tState.iJump = true;
			//iDirection = !iDirection;
			fBadAimTime = 0;
		}
        //return;
	}

    // TODO: Aim in the right direction to account of weapon speed, gravity and worm velocity

	fBadAimTime = 0;

    // Shoot
	if (tLX->fCurTime-fLastShoot > 0.005f)  {  // Don't shoot so often
		tState.iShoot = true;
		fLastShoot = tLX->fCurTime;
	}
}


///////////////////
// AI: Get the best weapon for the situation
// Returns weapon id or -1 if no weapon is suitable for the situation
int CWorm::AI_GetBestWeapon(int nGameType, float fDistance, bool bDirect, CMap *pcMap, float fTraceDist)
{
    // Note: This assumes that the weapons are the same as when we started the game
    // We could have picked up bonuses, but we will assume we havn't
    // (which will lead to interesting game scenarios if we have)


    // We need to wait a certain time before we change weapon
    /*float ChangeWeaponDelay[4] = {2, 1, 0.5f, 0.25f};

    if( tLX->fCurTime - fLastWeaponSwitch > ChangeWeaponDelay[tProfile->nDifficulty] )
        fLastWeaponSwitch = tLX->fCurTime;
    else
        return iCurrentWeapon;*/

	// For rifles and mortars just get the first unreloaded weapon
	if (iAiGameType == GAM_RIFLES || iAiGameType == GAM_MORTARS)  {
		for (int i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				return i;
		return 0;
	}

    CVec    cTrgPos = AI_GetTargetPos();
  /*  int     cx, cy;
    int     x, y;*/


	if (iAiGameType == GAM_100LT)  {
		// We're above the worm
		if (vPos.GetY() < (cTrgPos.GetY()-50.0f))  {
			// We're turned left
			if(iDirection == DIR_LEFT)  {
				// Aiming down
				int iFreeCells = 0;
				if (fAngle < 5)
					iFreeCells = IsEmpty(CELL_LEFT,pcMap) + IsEmpty(CELL_LEFTDOWN,pcMap) + IsEmpty(CELL_DOWN,pcMap);
				// Aiming up
				else
					iFreeCells = IsEmpty(CELL_LEFT,pcMap) + IsEmpty(CELL_LEFTUP,pcMap) + IsEmpty(CELL_UP,pcMap);

				// Ideal for napalm
				if (iFreeCells >= 3)
					if (!tWeapons[1].Reloading)
						return 1;
			}
			// We're turned right
			else
			{
				// Aiming down
				int iFreeCells = 0;
				if (fAngle > 180)
					iFreeCells = IsEmpty(CELL_CURRENT,pcMap) + IsEmpty(CELL_RIGHT,pcMap) + IsEmpty(CELL_RIGHTDOWN,pcMap) + IsEmpty(CELL_DOWN,pcMap);
				// Aiming up
				else
					iFreeCells = IsEmpty(CELL_CURRENT,pcMap) + IsEmpty(CELL_RIGHT,pcMap) + IsEmpty(CELL_RIGHTUP,pcMap) + IsEmpty(CELL_UP,pcMap);

				// Ideal for napalm
				if (iFreeCells >= 4)
					if (!tWeapons[1].Reloading)
						return 1;					
			}
		} // if (above worm)

		float d = CalculateDistance(vPos,cTrgPos);
		// We're close to the target
		if (d < 50.0f)  {
			// We see the target
			if(bDirect)  {
				// Super shotgun
				if (!tWeapons[0].Reloading)
					return 0;

				// Chaingun
				if (!tWeapons[4].Reloading)  
					return 4;
				

				// Doomsday
				if (!tWeapons[3].Reloading)
					return 3;

				// Let's try cannon
				if (!tWeapons[2].Reloading)
				// Don't use cannon when we're on ninja rope, we will avoid suicides
					if (!cNinjaRope.isReleased())  {
						// Aim a bit up
						AI_SetAim(CVec(cTrgPos.GetX(),cTrgPos.GetY()+5.0f));
						tState.iMove = false;  // Don't move, avoid suicides
						return 2;
					}
			}
			// We don't see the target
			else  {
				tState.iJump = true; // Jump, we might get better position
				return -1;
			}
		}

		// Not close, not far
		if (d > 50.0f && d<=300.0f)  {
			if (bDirect)  {

				// Chaingun is the best weapon for this situation
				if (!tWeapons[4].Reloading)  {
					return 4;
				}

				// Let's try cannon
				if (!tWeapons[2].Reloading)
					// Don't use cannon when we're on ninja rope, we will avoid suicides
					if (!cNinjaRope.isReleased())  {
						// Aim a bit up
						AI_SetAim(CVec(cTrgPos.GetX(),cTrgPos.GetY()+5.0f));
						tState.iMove = false;  // Don't move, avoid suicides
						return 2;
					}

				// Super Shotgun makes it sure
				if (!tWeapons[0].Reloading)
					return 0;

				// As for almost last, try doomsday
				if (!tWeapons[3].Reloading)
					// Don't use doomsday when we're on ninja rope, we will avoid suicides
					if (!cNinjaRope.isShooting())  {
						tState.iMove = false;  // Don't move, avoid suicides
						return 3;
					}
			} // End of direct shooting weaps

			// The worst case, napalm
			/*if (!tWeapons[1].Reloading)
				if (IsEmpty(CELL_CURRENT,pcMap) && IsEmpty(CELL_UP,pcMap))  {

					// Aim up
					if (iDirection == DIR_RIGHT)
						AI_SetAim(CVec(vPos.GetX()+10.0f,vPos.GetY()-10.0f));
					else 
						AI_SetAim(CVec(vPos.GetX()-10.0f,vPos.GetY()-10.0f));

					return 1;
				}*/

			return -1;
		}

		// Quite far
		if (d > 300.0f && bDirect)  {

			// First try doomsday
			if (!tWeapons[3].Reloading)  {
				// Don't use doomsday when we're on ninja rope, we will avoid suicides
				if (!cNinjaRope.isReleased())  {
					tState.iMove = false;  // Don't move, avoid suicides
					return 3;
				}
			}

			// Super Shotgun
			if (!tWeapons[0].Reloading)
				return 0;

			// Chaingun
			if (!tWeapons[4].Reloading)
				return 4;

			// Cannon, the worst possible for this
			if (!tWeapons[2].Reloading)
				// Don't use cannon when we're on ninja rope, we will avoid suicides
				if (!cNinjaRope.isReleased())  {
					// Aim a bit up
					AI_SetAim(CVec(cTrgPos.GetX(),cTrgPos.GetY()+5.0f));
					tState.iMove = false;  // Don't move, avoid suicides
					return 2;
				}
		}
					

		// The reloaded weapon, but not napalm
		/*for (int i=4; i>=0; i--)
			if (!tWeapons[i].Reloading && i!=1)
				return i;*/
		return -1;

	}

    /*
    0: "minigun"
	1: "shotgun"
	2: "chiquita bomb"
	3: "blaster"
	4: "big nuke"
    */


    /*
    
      Special firing cases
    
    */

    //
    // Case 1: The target is on the bottom of the level, a perfect spot to lob an indirect weapon
    //
    if(cTrgPos.GetY() > pcMap->GetHeight()-50 && fDistance < 200) {
        // Chiquita
        /*if(!tWeapons[2].Reloading)
            return 2;
        // Big nuke
        if(!tWeapons[4].Reloading)
            return 4;
        // Blaster
        if(!tWeapons[3].Reloading)
            return 3;*/
		for (int i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_SPECIAL)
					return i;
    }


    //
    // Case 2: We are very very close so we use the direct weapons
    //
    if(fDistance < 25 && bDirect) {
        // Shotgun
       /* if(!tWeapons[1].Reloading)
            return 1;
        // Minigun
        if(!tWeapons[0].Reloading)
            return 0;
        // Blaster
        if(!tWeapons[3].Reloading)
            return 3;*/
		for (int i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_PROJECTILE || tWeapons[i].Weapon->Type == WPN_BEAM)
					return i;
    }


    /*
    
      Direct firing weapons
    
    */

    //
    // Shotgun
    // If we're nice and close, use some beam or projectile weapon
    //
    if(fDistance < 100 && bDirect) {
        // First try beam
		int i;
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_BEAM)
					return i;

		// If beam not available, try projectile
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_PROJECTILE)
					return i;
    }


    //
    // Blaster
    // If we're at a medium distance, use any weapon, but prefer the exact ones
    //
    if(fDistance < 150 && bDirect) {

		// First try beam
		int i;
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_BEAM)
					return i;

		// If beam not available, try projectile
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_PROJECTILE)
					return i;

		// If everything fails, try any weapon
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				return i;
    }


    //
    // Any greater distance for direct firing uses a projectile weapon first
    //
    if(bDirect) {
        // Minigun
        /*if(!tWeapons[0].Reloading)
            return 0;
        // Shotgun
        if(!tWeapons[1].Reloading)
            return 1;*/
		// First try projectile
		int i;
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_PROJECTILE)
					return i;

		// If projectile not available, try beam
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				if (tWeapons[i].Weapon->Type == WPN_BEAM)
					return i;

		// If everything fails, try any weapon
		for (i=0; i<5; i++)
			if (!tWeapons[i].Reloading)  
				return i;
    }


    //
    // Indirect firing weapons
    //


    // If we're above the target, try any special weapon, for Liero mod try napalm
    // BUT only if our health is looking good
    // AND if there is no rock/dirt nearby
    if(fDistance > 190 && iHealth > 25 && fTraceDist > 0.5f && !bDirect && (cTrgPos.GetY()-20) > vPos.GetY()) {
        // Nuke
        /*if(!tWeapons[4].Reloading)
            return 4;
        // Chiquita
        if(!tWeapons[2].Reloading)
            return 2;*/
		if (strstr(tGameInfo.sModName,"Classic") || strstr(tGameInfo.sModName,"Liero"))
			for (int i=0; i<5; i++)
				if (!tWeapons[i].Reloading && strstr(tWeapons[i].Weapon->Name,"Napalm"))
					return i;

		for (int i=0; i<5; i++)
			if (!tWeapons[i].Reloading && tWeapons[i].Weapon->Type == WPN_SPECIAL)  
				return i;

    }


    //
    // Last resort
    //

    // Shoot a beam (we cant suicide with that)
	int i;
	for (i=0; i<5; i++)
		if (!tWeapons[i].Reloading && tWeapons[i].Weapon->Type == WPN_BEAM)  
			return i;
    
    
    
    // No suitable weapon found
	for (i=0;i<5;i++)
		if (!tWeapons[i].Reloading)
		{	
			return i;
		}

    return -1;
}


///////////////////
// Return any unloaded weapon in the list
/*int CWorm::cycleWeapons(void)
{
    // Don't do this in easy mode
    if( tProfile->nDifficulty == 0 )
        return iCurrentWeapon;

    // Find the first reloading weapon
    for( int i=0; i<5; i++) {
        if( tWeapons[i].Reloading )
            return i;
    }

    // Default case (all weapons loaded)
    return 0;
}*/


///////////////////
// Trace a line from this worm to the target
// Returns the distance the trace went
int CWorm::traceLine(CVec target, CMap *pcMap, float *fDist, int *nType, int divs)
{
    assert( pcMap );

    // Trace a line from the worm to length or until it hits something
	CVec    pos = vPos;
	CVec    dir = target-pos;
    int     nTotalLength = (int)NormalizeVector(&dir);

	int divisions = divs;			// How many pixels we go through each check (more = slower)
	if (tWeapons[iCurrentWeapon].Weapon->Type == WPN_BEAM)
		divisions = 1;

	if( nTotalLength < divisions)
		divisions = nTotalLength;

    *nType = PX_EMPTY;

	// Make sure we have at least 1 division
	divisions = MAX(divisions,1);

	// Rifle game
	if(iAiGameType == GAM_RIFLES && divisions == 5)  {
		divisions = 10;  // if we're close to a wall, we can shoot it through
		int i;
		for(i=0; i<nTotalLength; i+=divisions) {
			uchar px = pcMap->GetPixelFlag( (int)pos.GetX(), (int)pos.GetY() );

			if (i>10)  // we aren't close to a wall, so we can shoot through only thin wall
				divisions = 5;

			if(px & PX_DIRT || px & PX_ROCK) {
				*fDist = (float)i / (float)nTotalLength;
				*nType = px;
				return i;
			}

			pos = pos + dir * (float)divisions;
		}

		// Full length
		*fDist = (float)i / (float)nTotalLength;
		return nTotalLength;
	}

	// In mortars we can't shoot through wall
	if(iAiGameType == GAM_MORTARS && divisions == 5)
		divisions = 2;

	int i;
	for(i=0; i<nTotalLength; i+=divisions) {
		uchar px = pcMap->GetPixelFlag( (int)pos.GetX(), (int)pos.GetY() );
		//pcMap->PutImagePixel((int)pos.GetX(), (int)pos.GetY(), MakeColour(255,0,0));

        if(px & PX_DIRT || px & PX_ROCK) {
            *fDist = (float)i / (float)nTotalLength;
            *nType = px;
            return i;
        }

        pos = pos + dir * (float)divisions;
    }


    // Full length    
    *fDist = (float)i / (float)nTotalLength;
    return nTotalLength;
}

///////////////////
// Returns true, if the cell is empty
// Cell can be: CELL_CURRENT, CELL_LEFT,CELL_DOWN,CELL_RIGHT,CELL_UP,CELL_LEFTDOWN,CELL_RIGHTDOWN,CELL_LEFTUP,CELL_RIGHTUP
bool CWorm::IsEmpty(int Cell, CMap *pcMap)
{
  bool bEmpty = false;
  int cx = (int)(vPos.GetX() / pcMap->getGridWidth());
  int cy = (int)(vPos.GetY() / pcMap->getGridHeight());

  switch (Cell)  {
  case CELL_LEFT:
	  cx--;
	  break;
  case CELL_DOWN:
	  cy++;
	  break;
  case CELL_RIGHT:
	  cx++;
	  break;
  case CELL_UP:
	  cy--;
	  break;
  case CELL_LEFTDOWN:
	  cx--;
	  cy++;
	  break;
  case CELL_RIGHTDOWN:
	  cx++;
	  cy++;
	  break;
  case CELL_LEFTUP:
	  cx--;
	  cy--;
	  break;
  case CELL_RIGHTUP:
	  cx++;
	  cy--;
	  break;
  }

  if (cx < 0 || cx > pcMap->getGridCols())
	  return false;

  if (cy < 0 || cy > pcMap->getGridRows())
	  return false;

  /*int dx = cx*pcMap->getGridCols()*2;
  int dy = pcMap->getGridRows()*2*cy;

  DrawRect(pcMap->GetDrawImage(),dx,dy,dx+(pcMap->GetWidth()/pcMap->getGridCols())*2,dy+(pcMap->GetHeight()/pcMap->getGridRows())*2,0xFFFF);

  dx /= 2;
  dy /= 2;
  DrawRect(pcMap->GetImage(),dx,dy,dx+pcMap->GetWidth()/pcMap->getGridCols(),dy+pcMap->GetHeight()/pcMap->getGridRows(),0xFFFF);*/

  uchar   *f = pcMap->getGridFlags() + cy*pcMap->getGridWidth()+cx;
  bEmpty = *f == PX_EMPTY;

  return bEmpty;
}
