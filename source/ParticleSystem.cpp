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

#include "ParticleSystem.h"
//#include <stdlib.h>
//#include <math.h>

float random(float mean, float diff){
	float r = 2 * (rand() / float(RAND_MAX)) - 1.0f;
	
	return mean + r * fabsf(r) * diff;
}


ParticleSystem::ParticleSystem(){
	pos = vec3(0, 0, 0);
	directionalForce = vec3(0, 0, 0);
	
	spawnRate = 10;
	speed = 100;
	speedSpread = 25;
	
	size = 100;
	sizeSpread = 10;
	
	life = 2.5f;
	lifeSpread = 0.5f;
	
	frictionFactor = 0.7f;
	
	setColorScheme(COLOR_SCHEME_FIRE);

	lastTime = 0;
	particleCredit = 0;

	rotate = false;

	vertexArray = NULL;
	vertexArraySize = 0;
	indexArray = NULL;
	indexArraySize = 0;
}

ParticleSystem::~ParticleSystem(){
	delete vertexArray;
	delete indexArray;
}

void ParticleSystem::setColorScheme(const COLOR_SCHEME colorScheme){
	float f;
	int i;
	
	switch(colorScheme){
	case COLOR_SCHEME_FIRE:
		for (i = 0; i < 4; i++){
			colors[i    ] = vec4(i / 4.0f, 0, 0, 0);
			colors[i + 4] = vec4(1, i / 4.0f, 0, 0);
			colors[i + 8] = vec4((3 - i) / 3.0f, (3 - i) / 3.0f, 1, 0);
		}
		break;
	case COLOR_SCHEME_ICE:
		for (i = 0; i < 6; i++){
			colors[i    ] = vec4(0, 0, i / 6.0f, 0);
			colors[i + 6] = vec4(i / 5.0f, 1, 1, 0);
		}
		break;
	case COLOR_SCHEME_SMOKE:
		for (i = 0; i < 12; i++){
			f = i / 44.0f;
			colors[i] = vec4(f, f, f, f);
		}
		break;
	case COLOR_SCHEME_RAINBOW:
		colors[0]  = vec4(0,   0,   0,     0);
		colors[1]  = vec4(0,   0,   0.25f, 0);
		colors[2]  = vec4(0,   0,   0.5f,  0);
		colors[3]  = vec4(0,   0,   1,     0);
		colors[4]  = vec4(0,   0.5f,1,     0);
		colors[5]  = vec4(0,   1,   1,     0);
		colors[6]  = vec4(0,   1,   0.5f,  0);
		colors[7]  = vec4(0,   1,   0,     0);
		colors[8]  = vec4(0.5f,1,   0,     0);
		colors[9]  = vec4(1,   1,   0,     0);
		colors[10] = vec4(1,   0.5f,0,     0);
		colors[11] = vec4(1,   0,   0,     0);
		break;
	}
}

void ParticleSystem::initParticle(Particle &p){
	p.pos = pos;
	p.dir = normalize(vec3(random(0, 0.3f), 1, random(0, 0.3f)));
	p.dir *= random(speed, speedSpread);
	p.size = random(size, sizeSpread);
	/*p.initialLife = */p.life = random(life, lifeSpread);
	p.invInitialLife = 1.0f / p.life;
}

void ParticleSystem::updateParticle(Particle &p, const float time){
	p.pos += p.dir * time;
}

void ParticleSystem::update(const float timeStamp){
	Particle p;
	float time, dist, friction;
	unsigned int i, j, len;

	time = timeStamp - lastTime;
	lastTime = timeStamp;

	particleCredit += time * spawnRate;
	len = (int) particleCredit;
	particleCredit -= len;

	for (i = 0; i < len; i++){
		initParticle(p);
		particles.add(p);
	}

	friction = powf(frictionFactor, time);

	i = 0;
	while (i < particles.getCount()){
		if ((particles[i].life -= time) < 0){
			particles.remove(i);
			continue;
		}

		vec3 v(0, 0, 0);
		for (j = 0; j < pointForces.getCount(); j++){
			vec3 dir = pointForces[j].pos - particles[i].pos;
			dist = dot(dir, dir);
			v += dir * (pointForces[j].strength / (1.0f + sqrtf(dist) * pointForces[j].linearAttenuation + dist * pointForces[j].quadraticAttenuation));
		}

		particles[i].dir += (directionalForce + v) * time;
		particles[i].dir *= friction;
		if (rotate) particles[i].angle += particles[i].angleSpeed * time;

		//particles[i].pos += particles[i].dir * time;
		updateParticle(particles[i], time);

		i++;
	}
}

void ParticleSystem::updateTime(const float timeStamp){
	lastTime = timeStamp;
}

int depthComp(const Particle &elem0, const Particle &elem1){
	return (elem0.depth < elem1.depth)? 1 : -1;
}

void ParticleSystem::depthSort(const vec3 &pos, const vec3 &depthAxis){
	for (unsigned int i = 0; i < particles.getCount(); i++){
		particles[i].depth = fabsf(dot(particles[i].pos - pos, * depthAxis));
	}
	
	particles.sort(depthComp);
}

char *ParticleSystem::getVertexArray(const vec3 &dx, const vec3 &dy, bool useColors, bool tex3d){
	unsigned int vertexSize = sizeof(vec3) + sizeof(vec2);
	if (useColors) vertexSize += sizeof(vec4);
	if (tex3d) vertexSize += sizeof(float);
	unsigned int size = particles.getCount() * vertexSize * 4;

	if (size > vertexArraySize){
		delete vertexArray;
		vertexArray = new char[size];
		vertexArraySize = size;
	}

	fillVertexArray(vertexArray, dx, dy, useColors, tex3d);

	return vertexArray;
}

char *ParticleSystem::getPointSpriteArray(bool useColors){
	unsigned int vertexSize = sizeof(vec3) + sizeof(float);
	if (useColors) vertexSize += sizeof(vec4);
	unsigned int size = vertexSize * particles.getCount();

	if (size > vertexArraySize){
		delete vertexArray;
		vertexArray = new char[size];
		vertexArraySize = size;
	}

	char *dest = vertexArray;
	for (unsigned int i = 0; i < particles.getCount(); i++){
		*(vec3 *) dest = particles[i].pos;
		dest += sizeof(vec3);
		*(float *) dest = particles[i].size;
		dest += sizeof(float);

		if (useColors){
			//float colFrac = (11.0f * particles[i].life) / particles[i].initialLife;
			float colFrac = 11.0f * particles[i].life * particles[i].invInitialLife;

			int colInt = (int) colFrac;
			colFrac -= colInt;

			*(vec4 *) dest = lerp(colors[colInt], colors[colInt + 1], colFrac);
			dest += sizeof(vec4);
		}
	}

	return vertexArray;
}

unsigned short *ParticleSystem::getIndexArray(){
	unsigned int size = particles.getCount() * 6;

	if (size > indexArraySize){
		delete indexArray;
		indexArray = new unsigned short[size];
		indexArraySize = size;

		fillIndexArray(indexArray);
	}

	return indexArray;
}

void ParticleSystem::fillVertexArray(char *dest, const vec3 &dx, const vec3 &dy, bool useColors, bool tex3d){
	static vec2 coords[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
	vec3 vect[4] = { -dx + dy, dx + dy, dx - dy, -dx - dy };

	float frac = 0;
	vec4 color;
	for (unsigned int i = 0; i < particles.getCount(); i++){
		if (useColors || tex3d)
			frac = particles[i].life * particles[i].invInitialLife;
//			frac = particles[i].life / particles[i].initialLife;

		if (useColors){
			float colFrac = 11.0f * frac;
			int colInt = (int) colFrac;
			colFrac -= colInt;

			color = lerp(colors[colInt], colors[colInt + 1], colFrac);
		}

		if (rotate){
			float fx = 1.4142136f * cosf(particles[i].angle);
			float fy = 1.4142136f * sinf(particles[i].angle);
		
			for (unsigned int k = 0; k < 4; k++){
				vect[k] = fx * dx + fy * dy;

				float t = fy;
				fy = -fx;
				fx = t;
			}
		}

		for (unsigned int j = 0; j < 4; j++){
			*(vec3 *) dest = particles[i].pos + particles[i].size * vect[j];
			dest += sizeof(vec3);
			*(vec2 *) dest = coords[j];
			dest += sizeof(vec2);

			if (tex3d){
				*(float *) dest = 1.0f - frac;
				dest += sizeof(float);
			}

			if (useColors){
				*(vec4 *) dest = color;
				dest += sizeof(vec4);
			}
		}
	}
}

void ParticleSystem::fillInstanceVertexArray(char *dest){
	Particle *part = particles.getArray();
	for (unsigned int i = 0; i < particles.getCount(); i++){
		*(vec3 *) dest = part->pos;
		dest += sizeof(vec3);
		*(float *) dest = part->size;
		dest += sizeof(float);

		float colFrac = 11.0f * part->life * part->invInitialLife;
		int colInt = (int) colFrac;
		colFrac -= colInt;

		*(vec4 *) dest = lerp(colors[colInt], colors[colInt + 1], colFrac);
		dest += sizeof(vec4);

		part++;
	}
}

void ParticleSystem::fillInstanceVertexArrayRange(vec4 *posAndSize, vec4 *color, const unsigned int start, unsigned int count){
	Particle *part = particles.getArray() + start;

	for (unsigned int i = 0; i < count; i++){
		*(vec3 *) posAndSize = part->pos;
		posAndSize->w = part->size;
		posAndSize++;

		float colFrac = 11.0f * part->life * part->invInitialLife;
		int colInt = (int) colFrac;
		colFrac -= colInt;

		*color++ = lerp(colors[colInt], colors[colInt + 1], colFrac);

		part++;
	}
}

void ParticleSystem::fillIndexArray(unsigned short *dest){
	for (unsigned int i = 0; i < particles.getCount(); i++){
		*dest++ = 4 * i;
		*dest++ = 4 * i + 1;
		*dest++ = 4 * i + 3;
		*dest++ = 4 * i + 3;
		*dest++ = 4 * i + 1;
		*dest++ = 4 * i + 2;
	}
}
