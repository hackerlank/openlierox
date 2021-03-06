/*
 *  CGameObject.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 27.07.09.
 *  code under LGPL
 *
 */

#include "game/CGameObject.h"
#include "game/Game.h"
#include "CGameScript.h"
#include "game/CWorm.h"
#include "PhysicsLX56.h"
#include "CVec.h"
#include "util/angle.h"
#include "game/WormInputHandler.h"
#include "gusanos/luaapi/context.h"
#include "gusanos/lua/bindings-objects.h"

LuaReference CGameObject::metaTable;

CGameObject::CGameObject() { gusSpeedScope = false; gusInit(); }
CGameObject::CGameObject(CWormInputHandler* owner, Vec pos_, Vec spd_) { gusSpeedScope = false; gusInit(owner, pos_, spd_); }
CGameObject::~CGameObject() { gusShutdown(); }


bool CGameObject::canUpdateAttribs(const BaseObject* obj, const AttrDesc* attrDesc) {
	const CWorm* w = dynamic_cast<const CWorm*>(obj);
	if(!w) return true; // not yet implemented/handled. pass on to other checks in Attr code
	if(!w->getAlive()) return game.isServer();
	return w->getLocal();
}

bool CGameObject::injure(float damage) {
	health -= damage;

	if(health < 0.0f) {
		health = 0.0f;
		return true;
	}

	return false;
}

void CGameObject::gusInit( CWormInputHandler* owner, Vec pos_, Vec spd_ )
{
	nextS_=(0); nextD_=(0); prevD_=(0); cellIndex_=(-1);
	deleteMe=(false);

	m_owner=(owner);
	vPos = CVec(pos_);
	vVelocity = CVec(spd_);
}

void CGameObject::gusShutdown()
{
	luaData.destroy();
}

Vec CGameObject::getRenderPos()
{
	return pos();
}

Angle CGameObject::getPointingAngle()
{
	return Angle(0);
}

int CGameObject::getDir()
{
	return 1;
}

CWormInputHandler* CGameObject::getOwner()
{
	return m_owner;
}

void CGameObject::remove()
{
	deleteMe = true;
}

bool CGameObject::isCollidingWith( const Vec& point, float radius )
{
	return (Vec(pos()) - point).lengthSqr() < radius*radius;
}

void CGameObject::removeRefsToPlayer(CWormInputHandler* player)
{
	if ( m_owner == player )
		m_owner = NULL;
}

bool CGameObject::isInside(int x, int y) const {
	IVec s = size();
	return abs((int)pos().get().x - x) + 1 <= s.x && abs((int)pos().get().y - y) + 1 <= s.y;
}



// -------
// GusDT() default is 1/100 = 0.01.
#define GusDT	TimeDiff(Game::FixedFrameTime)

CGameObject::ScopedGusCompatibleSpeed::ScopedGusCompatibleSpeed(CGameObject& o) : obj(o) {
	// Note: Instead of having asserts on obj.gusSpeedScope, we make it dynamic.
	// This is because the Attr::write() fallback refers always to the same
	// static object and thus we might get messed up, setting the scope twice
	// or so on this static dummy object. But that doesn't realy matter anyway.
	if(!obj.gusSpeedScope) {
		// We do this if we use the LX56 Physics simulation on worms
		// Gusanos interprets the velocity in a different way, so we convert it while we are doing Gus stuff.
		// Gusanos uses velocity as follows: pos += velocity (per frame, with 100 FPS).
		obj.velocity() *= GusDT.seconds();
		obj.gusSpeedScope = true;
	}
}

CGameObject::ScopedGusCompatibleSpeed::~ScopedGusCompatibleSpeed() {
	if(obj.gusSpeedScope) {
		obj.velocity() *= 1.0f / GusDT.seconds();
		obj.gusSpeedScope = false;
	}
}

CGameObject::ScopedLXCompatibleSpeed::ScopedLXCompatibleSpeed(CGameObject& o) : obj(o) {
	if(obj.gusSpeedScope) {
		obj.velocity() *= 1.0f / GusDT.seconds();
		obj.gusSpeedScope = false;
	}
}

CGameObject::ScopedLXCompatibleSpeed::~ScopedLXCompatibleSpeed() {
	if(!obj.gusSpeedScope) {
		obj.velocity() *= GusDT.seconds();
		obj.gusSpeedScope = true;
	}
}

float convertSpeed_LXToGus(float v) {
	return v * GusDT.seconds();
}

float convertSpeed_GusToLX(float v) {
	return v / GusDT.seconds();
}

float convertAccel_LXToGus(float v) {
	return v * GusDT.seconds() * GusDT.seconds();
}

float convertAccel_GusToLX(float v) {
	return (v / GusDT.seconds()) / GusDT.seconds();
}

CVec CGameObject::getGusVel() const {
	if(gusSpeedScope) return getVelocity();
	return CVec(convertSpeed_LXToGus(vVelocity.get().x),
				convertSpeed_LXToGus(vVelocity.get().y));
}

void CGameObject::setGusVel(CVec v) {
	if(gusSpeedScope) velocity() = v;
	else {
		ScopedGusCompatibleSpeed speedScope(*this);
		velocity() = v;
	}
}


REGISTER_CLASS(CGameObject, LuaID<CustomVar>::value)
