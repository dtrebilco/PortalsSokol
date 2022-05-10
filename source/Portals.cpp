#if 0

#include "Portals.h"

Sector::Sector(OpenGLModel *roomModel){
	room = roomModel;

	room->getBoundingBox((float *) &min, (float *) &max);
}

Sector::~Sector(){
	delete room;

	for (unsigned int i = 0; i < lights.getCount(); i++){
		delete lights[i].particles;
	}
}

void Sector::init(){
	for (unsigned int i = 0; i < portals.getCount(); i++){
		glGenQueriesARB(1, &portals[i].query);
	}
}

void Sector::clear(){
	for (unsigned int i = 0; i < portals.getCount(); i++){
		glDeleteQueriesARB(1, &portals[i].query);
	}
}

void Sector::addPortal(Portal portal){
	portals.add(portal);
}

void Sector::addLight(const vec3 &pos, const float rad, const float xs, const float ys, const float zs){
	Light light;

	light.position = pos;
	light.radius = rad;
	light.xs = xs;
	light.ys = ys;
	light.zs = zs;

	ParticleSystem *particles = new ParticleSystem();
	particles->setSpawnRate(400);
	particles->setSpeed(70, 20);
	particles->setLife(3.0f, 0);
	particles->setDirectionalForce(vec3(0, -10, 0));
	particles->setFrictionFactor(0.95f);
	//particles->setPosition(pos);
	particles->setSize(15, 5);

	for (unsigned int i = 0; i < 6; i++){
		particles->setColor(i, vec4(0.05f * i, 0.01f * i, 0, 0));
		particles->setColor(6 + i, vec4(0.05f * 6, 0.05f * i + 0.06f, 0.02f * i, 0));
	}

	light.particles = particles;

	lights.add(light);
}

bool Sector::isInBoundingBox(vec3 &pos) const {
	return (pos.x > min.x && pos.x < max.x &&
			pos.y > min.y && pos.y < max.y &&
			pos.z > min.z && pos.z < max.z);
}

bool Sector::isSphereInSector(const vec3 &pos, const float radius) const {
	return (getDistanceSqr(pos) < radius * radius);
}

float Sector::getDistanceSqr(const vec3 &pos) const {
	float s, d = 0;
	for (int i = 0; i < 3; i++){
		if (pos[i] < min[i]){
			s = pos[i] - min[i];
			d += s * s;
		} else if (pos[i] > max[i]){
			s = pos[i] - max[i];
			d += s * s;
		}
	}
	return d;
}

void Sector::draw(Renderer *renderer, ShaderID shader, TextureID *base, TextureID *bump, vec3 &position, vec3 &dx, vec3 &dy, bool useOcclusionQueries){
	unsigned int i, j;

	glEnable(GL_CULL_FACE);

	for (j = 0; j < lights.getCount(); j++){
		float t = time - 0.1f;
		vec3 p = vec3(lights[j].xs * cosf(4.23f * t + j), lights[j].ys * sinf(2.37f * t) * cosf(1.39f * t), lights[j].zs * sinf(3.12f * t + j));

		for (i = 0; i < room->getBatchCount(); i++){
			renderer->setShader(shader);
			renderer->setTexture("Base", base[i]);
			renderer->setTexture("Bump", bump[i]);
			if (j > 0) renderer->setBlending(ONE, ONE);
			renderer->apply();

			renderer->changeShaderConstant3f("lightPos", lights[j].position + p);
			renderer->changeShaderConstant3f("camPos", position);
			renderer->changeShaderConstant1f("invRadius", 1.0f / lights[j].radius);
			renderer->changeShaderConstant1f("ambient", (j > 0)? 0 : 0.07f);

			((OpenGLBatch *) room->getBatch(i))->draw();
		}
	}

	hasBeenDrawn = true;

	if (useOcclusionQueries){
		glDisable(GL_CULL_FACE);

		renderer->setMask(NONE);
		renderer->apply();

		for (i = 0; i < portals.getCount(); i++){
			if (portals[i].sector->hasBeenDrawn) continue;

			glBeginQueryARB(GL_SAMPLES_PASSED_ARB, portals[i].query);
			glBegin(GL_QUADS);
				glVertex3fv(portals[i].v0);
				glVertex3fv(portals[i].v1);
				glVertex3fv(portals[i].v2);
				glVertex3fv(portals[i].v3);
			glEnd();
			glEndQueryARB(GL_SAMPLES_PASSED_ARB);
		}

		glFlush();
	}

	// Do something useful while waiting for portals rendering to complete
	for (j = 0; j < lights.getCount(); j++){
		vec3 p = vec3(lights[j].xs * cosf(4.23f * time + j), lights[j].ys * sinf(2.37f * time) * cosf(1.39f * time), lights[j].zs * sinf(3.12f * time + j));
		lights[j].particles->setPosition(lights[j].position + p);
		lights[j].particles->update(time);
		lights[j].vertexArray = lights[j].particles->getVertexArray(dx, dy);
	}

	if (useOcclusionQueries){
		for (i = 0; i < portals.getCount(); i++){
			if (portals[i].sector->hasBeenDrawn) continue;

			GLuint samples = 0;
			glGetQueryObjectuivARB(portals[i].query, GL_QUERY_RESULT_ARB, &samples);

			if (samples > 100){
				portals[i].sector->draw(renderer, shader, base, bump, position, dx, dy, useOcclusionQueries);
			}
		}
	} else {
		for (i = 0; i < portals.getCount(); i++){
			if (!portals[i].sector->hasBeenDrawn){
				portals[i].sector->draw(renderer, shader, base, bump, position, dx, dy, useOcclusionQueries);
			}
		}
	}
}

void Sector::drawParticles(Renderer *renderer, TextureID particle){
	if (hasBeenDrawn){
		renderer->setTextures(particle);
		renderer->setBlending(ONE, ONE);
		renderer->setMask(COLOR);
		renderer->apply();

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		for (unsigned int j = 0; j < lights.getCount(); j++){
			glVertexPointer  (3, GL_FLOAT, sizeof(vec3) + sizeof(vec2) + sizeof(vec4), lights[j].vertexArray);
			glTexCoordPointer(2, GL_FLOAT, sizeof(vec3) + sizeof(vec2) + sizeof(vec4), lights[j].vertexArray + sizeof(vec3));
			glColorPointer   (4, GL_FLOAT, sizeof(vec3) + sizeof(vec2) + sizeof(vec4), lights[j].vertexArray + sizeof(vec3) + sizeof(vec2));

			glDrawArrays(GL_QUADS, 0, lights[j].particles->getParticleCount() * 4);
		}
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	} else {
		for (unsigned int j = 0; j < lights.getCount(); j++){
			lights[j].particles->updateTime(time);
		}
	}
}
#endif