/*
 *  CaptureTheFlag.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 20.03.09.
 *  code under LGPL
 *
 */

#include "CGameMode.h"
#include "CServer.h"
#include "CWorm.h"
#include "FlagInfo.h"

struct CaptureTheFlag : public CGameMode {
	
	virtual std::string Name() {
		return "Capture The Flag";
	}

	virtual GameInfoGroup getGameInfoGroupInOptions() { return GIG_CaptureTheFlag; }

	static const int MAXTEAMS = 4;
	int teamScore[MAXTEAMS];
	bool flagsSpawned[MAXTEAMS];
	bool gameStarted;
	
	virtual int GameTeams() {
		return MAXTEAMS;
	}

	virtual int TeamScores(int t) {
		if(t >= 0 && t < MAXTEAMS) return teamScore[t];
		return -1;
	}

	virtual int CompareTeamScore(int t1, int t2) {
		// team score is most important in CTF, more important than left lifes
		{
			int d = TeamScores(t1) - TeamScores(t2);
			if(d != 0) return d;
		}
		
		// ok, fallback to default
		return CGameMode::CompareTeamsScore(t1, t2);
	}

	CaptureTheFlag() { reset(); }
	
	void reset() {
		for(int i = 0; i < MAXTEAMS; ++i)
		{
			teamScore[i] = 0;
			flagsSpawned[i] = false;
		}
		gameStarted = false;
	}
	
	bool isTeamUsed(int t) {
		return !cServer->isTeamEmpty(t);
	}
	
	void initFlag(CWorm * worm) {
		Flag * ff = cServer->flagInfo()->applyInitFlag(worm->getTeam(), worm->getPos());
		cServer->flagInfo()->applyHolderWorm(ff, worm->getID());
		cServer->SendGlobalText(worm->getName() + " - set base position for " + flagName(ff->id), TXT_NORMAL);
	}
	
	bool Shoot(CWorm* worm) 
	{
		if( worm->getClient() == NULL ) // This function is called from CClient from weapon selection screen (which is hax IMO)
			return true;
			
		if( !gameStarted )
		{
			if( cServer->flagInfo()->getFlagOfWorm(worm->getID()) != NULL )
			{
				cServer->flagInfo()->applyInitFlag(worm->getTeam(), worm->getPos());
				cServer->SendGlobalText(flagName(worm->getTeam()) + " position is set", TXT_NORMAL);
				flagsSpawned[worm->getTeam()] = true;
			}
			bool allFlagsSet = true;
			for(int i = 0; i < MAXTEAMS; ++i)
				if( ! flagsSpawned[i] && isTeamUsed(i) )
					allFlagsSet = false;
			if( allFlagsSet )
			{
				gameStarted = true;
				cServer->SendGlobalText("All flags are set, game is started", TXT_NORMAL);
			}
			return false;
		}
		return true;
	}

	void Drop(CWorm* worm) 
	{
		if( !gameStarted )
		{
			if( cServer->flagInfo()->getFlagOfWorm(worm->getID()) != NULL )
			{
				// Pass flag to another worm from the same team
				for(int i = 0; i < MAX_WORMS; i++)
				{
					CWorm * w = & (cServer->getWorms()[i]);
					if( w->isUsed() && w->getTeam() == worm->getTeam() && w->getID() != worm->getID() )
					{
						initFlag(w);
						return;
					}
				}
			}
		}
	}
	
	std::string flagName(int t) {
		return TeamName(t) + " flag";
	}
	
	virtual void PrepareGame() {
		CGameMode::PrepareGame();
		reset();
	}
	
	void wormCatchFlag(CWorm* worm, Flag* flag) {
		if(!(bool)tLXOptions->tGameInfo.features[FT_CTF_AllowRopeForCarrier])
			cServer->SetWormCanUseNinja(worm->getID(), false);
		cServer->SetWormSpeedFactor(worm->getID(), tLXOptions->tGameInfo.features[FT_CTF_SpeedFactorForCarrier]);
	}
	
	void wormLooseFlag(CWorm* worm, Flag* flag) {
		if(!(bool)tLXOptions->tGameInfo.features[FT_CTF_AllowRopeForCarrier])
			cServer->SetWormCanUseNinja(worm->getID(), true);
		cServer->SetWormSpeedFactor(worm->getID(), tLXOptions->tGameInfo.features[FT_WormSpeedFactor]);		
	}
	
	virtual bool Spawn(CWorm* worm, CVec pos) {
		if(!CGameMode::Spawn(worm, pos)) return false;
		
		if(!cServer->flagInfo()->getFlag(worm->getTeam())) {
			// we have to create the new flag, there isn't any yet
			initFlag(worm);
		}
		return true;
	}
	
	virtual void Kill(CWorm* victim, CWorm* killer) {
		if( gameStarted )
		{
			Flag* victimsFlag = cServer->flagInfo()->getFlagOfWorm(victim->getID());
			if(victimsFlag) {
				wormLooseFlag(victim, victimsFlag);
				CVec pos = cServer->getMap()->groundPos(victim->getPos()) - CVec(0,(float)(cServer->flagInfo()->getHeight()/4));
				cServer->flagInfo()->applyCustomPos(victimsFlag, pos);
				cServer->SendGlobalText(victim->getName() + " lost " + flagName(victimsFlag->id), TXT_NORMAL);			
			}
		}
		
		if(killer && killer != victim) {
			killer->addKill();
		}
		
		// Victim is out of the game
		if(victim->Kill() && networkTexts->sPlayerOut != "<none>")
			cServer->SendGlobalText(replacemax(networkTexts->sPlayerOut, "<player>",
											   victim->getName(), 1), TXT_NORMAL);
	}
	
	virtual void hitFlag(CWorm* worm, Flag* flag) {
		if( !gameStarted )
			return;

		if(flag->id == worm->getTeam()) { // own flag
			if(!flag->atSpawnPoint && flag->holderWorm < 0) {
				cServer->flagInfo()->applySetBack(flag);
				cServer->SendGlobalText(worm->getName() + " returned " + flagName(flag->id), TXT_NORMAL);
			}
		}
		else { // enemy flag
			cServer->flagInfo()->applyHolderWorm(flag, worm->getID());
			cServer->SendGlobalText(worm->getName() + " caught " + flagName(flag->id), TXT_NORMAL);
			wormCatchFlag(worm, flag);
		}
	}
	
	virtual void hitFlagSpawnPoint(CWorm* worm, Flag* flag) {
		if( !gameStarted )
			return;

		if(worm->getTeam() == flag->id) { // own base
			Flag* wormsFlag = cServer->flagInfo()->getFlagOfWorm(worm->getID());
			if(wormsFlag && flag->atSpawnPoint) {
				// yay, we scored!
				teamScore[worm->getTeam()]++;
				wormLooseFlag(worm, wormsFlag);
				cServer->flagInfo()->applySetBack(wormsFlag);
				cServer->SendGlobalText(worm->getName() + " scored for " + TeamName(worm->getTeam()), TXT_NORMAL);
				cServer->SendTeamScoreUpdate();
				cServer->RecheckGame();
			}
		}
	}	
	
	
};

static CaptureTheFlag gameMode;
CGameMode* gameMode_CaptureTheFlag = &gameMode;


