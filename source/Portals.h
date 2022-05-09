#ifndef _PORTALS_H_
#define _PORTALS_H_

#include "../Framework2/Util/OpenGLModel.h"
#include "../Framework2/OpenGL/OpenGLRenderer.h"
#include "../Framework2/Util/ParticleSystem.h"

struct Light {
	ParticleSystem *particles;
	char *vertexArray;

	vec3 position;
	float radius;
	float xs, ys, zs;
};

class Sector;

struct Portal {
	Portal(Sector *sect, const vec3 &vc0, const vec3 &vc1, const vec3 &vc2){
		sector = sect;
		v0 = vc0;
		v1 = vc1;
		v2 = vc1 + vc2 - vc0;
		v3 = vc2;
		query = 0;
	}

	vec3 v0, v1, v2, v3;
	Sector *sector;
	GLuint query;
};



class Sector {
public:
	Sector(OpenGLModel *roomModel);
	~Sector();

	void init();
	void clear();

	void addPortal(Portal portal);
	void addLight(const vec3 &pos, const float rad, const float xs, const float ys, const float zs);

	bool isInBoundingBox(vec3 &pos) const;
	bool isSphereInSector(const vec3 &pos, const float radius) const;
	float getDistanceSqr(const vec3 &pos) const;

	void draw(Renderer *renderer, ShaderID shader, TextureID *base, TextureID *bump, vec3 &position, vec3 &dx, vec3 &dy, bool useOcclusionQueries);
	void drawParticles(Renderer *renderer, TextureID particle);

	void clearDrawFlag(){ hasBeenDrawn = false; }
	bool getDrawnFlag() const { return hasBeenDrawn; }

private:
	OpenGLModel *room;
	Set <Portal> portals;
	Set <Light> lights;

	vec3 min, max;

	bool hasBeenDrawn;
};


#endif // _PORTALS_H_
