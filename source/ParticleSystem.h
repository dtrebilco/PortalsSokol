/***********      .---.         .-"-.      *******************\
* -------- *     /   ._.       / ´ ` \     * ---------------- *
* Author's *     \_  (__\      \_°v°_/     * humus@rogers.com *
*   note   *     //   \\       //   \\     * ICQ #47010716    *
* -------- *    ((     ))     ((     ))    * ---------------- *
*          ****--""---""-------""---""--****                  ********\
* This file is a part of the work done by Humus. You are free to use  *
* the code in any way you like, modified, unmodified or copy'n'pasted *
* into your own work. However, I expect you to respect these points:  *
*  @ If you use this file and its contents unmodified, or use a major *
*    part of this file, please credit the author and leave this note. *
*  @ For use in anything commercial, please request my approval.      *
*  @ Share your work and ideas too as much as you can.                *
\*********************************************************************/

#ifndef _PARTICLESYSTEM_H_
#define _PARTICLESYSTEM_H_

#include "../Math/Vector.h"
#include "Set.h"

enum COLOR_SCHEME {
	COLOR_SCHEME_FIRE,
	COLOR_SCHEME_ICE,
	COLOR_SCHEME_SMOKE,
	COLOR_SCHEME_RAINBOW
};


struct Particle {
	vec3 pos;
	float size;

	vec3 dir;
	float life;
	float invInitialLife;

	float depth;

	float angle;
	float angleSpeed;
};

struct PointForce {
	vec3 pos;
	float strength;
	float linearAttenuation;
	float quadraticAttenuation;
};

float random(float mean, float diff);

class ParticleSystem {
public:
	ParticleSystem();
	virtual ~ParticleSystem();

	const vec3 &getPosition() const { return pos; }
	unsigned int getParticleCount() const { return particles.getCount(); }
	void setPosition(const vec3 &position){ pos = position; }
	void setSpawnRate(const float spawnrate){ spawnRate = spawnrate; }

	void setSpeed(const float meanSpeed, const float spread){
		speed = meanSpeed;
		speedSpread = spread;
	}

	void setLife(const float meanLife, const float spread){
		life = meanLife;
		lifeSpread = spread;
	}

	void setSize(const float meanSize, const float spread){
		size = meanSize;
		sizeSpread = spread;
	}

	void setDirectionalForce(const vec3 &df){ directionalForce = df; }
	void addPointForce(const PointForce &pf){ pointForces.add(pf); }
	void setPointForce(const int force, const PointForce &pf){ pointForces[force] = pf;	}
	void setFrictionFactor(const float friction){ frictionFactor = friction; }

	void setColor(const int color, const vec4 &col){ colors[color] = col; }
	void setColorScheme(const COLOR_SCHEME colorScheme);

	void setRotate(const bool rot){ rotate = rot; }

	void update(const float timeStamp);
	void updateTime(const float timeStamp);
	void depthSort(const vec3 &pos, const vec3 &depthAxis);

	char *getVertexArray(const vec3 &dx, const vec3 &dy, bool useColors = true, bool tex3d = false);
	char *getPointSpriteArray(bool useColors = true);
	unsigned short *getIndexArray();

	void fillVertexArray(char *dest, const vec3 &dx, const vec3 &dy, bool useColors = true, bool tex3d = false);
	void fillInstanceVertexArray(char *dest);
	void fillInstanceVertexArrayRange(vec4 *posAndSize, vec4 *color, const unsigned int start, unsigned int count);
	void fillIndexArray(unsigned short *dest);

protected:
	virtual void initParticle(Particle &p);
	virtual void updateParticle(Particle &p, const float time);

	Set <Particle> particles;
	Set <PointForce> pointForces;
	vec3 directionalForce;
	
	float lastTime, particleCredit;

	vec4 colors[12];

	vec3 pos;

	float spawnRate;
	float speed, speedSpread;
	float size, sizeSpread;
	float life, lifeSpread;
	float frictionFactor;

	bool rotate;

	char *vertexArray;
	unsigned int vertexArraySize;
	unsigned short *indexArray;
	unsigned int indexArraySize;
};


#endif // _PARTICLESYSTEM_H_
