#include "../Framework2/OpenGL/OpenGLApp.h"
#include "Portals.h"

class MainApp : public OpenGLApp {
public:
	void initMenu();
	void resetCamera();

	bool init();
	bool exit();
	bool load();
	bool unload();

	bool drawFrame();
protected:
	Set <Sector *> sectors;

	TextureID shader;
	TextureID base[3];
	TextureID bump[3];
	TextureID particle;

	bool useOcclusionQueries;
};

void MainApp::initMenu(){
	Menu *menu = menuSystem->getMainMenu();
	menu->addMenuItem("Use occlusion queries: ", &useOcclusionQueries, INPUT_BOOL);

	App::initMenu();
}

void MainApp::resetCamera(){
	position = vec3(470, 220, 210);
	wx = 0;
	wy = PI / 2;
	wz = 0;
}

bool MainApp::init(){
	useOcclusionQueries = true;

	char fileName[] = "room0.hmdl";
	unsigned int i;

	OpenGLModel *rooms[5];

	for (i = 0; i < 5; i++){
		fileName[4] = '0' + i;

		rooms[i] = new OpenGLModel();
		rooms[i]->loadFromFile(fileName);
	}

	mat4 matr0(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	matr0.translate(vec3(0, 256, 0));

	mat4 matr1(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	matr1.translate(vec3(-384, 256, 3072));

	mat4 matr2(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	matr2.translate(vec3(1536,256,2688));

	mat4 matr3(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	matr3.translate(vec3(-1024,-768,2688));

	mat4 matr4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
	matr4.translate(vec3(-2304,256,2688));

	for (i = 0; i < rooms[1]->getBatchCount(); i++){
		rooms[0]->getBatch(i)->transform(matr0, ATT_VERTEX);
		rooms[1]->getBatch(i)->transform(matr1, ATT_VERTEX);
		rooms[2]->getBatch(i)->transform(matr2, ATT_VERTEX);
		rooms[3]->getBatch(i)->transform(matr3, ATT_VERTEX);
		rooms[4]->getBatch(i)->transform(matr4, ATT_VERTEX);
	}


	for (i = 0; i < 5; i++)	sectors.add(new Sector(rooms[i]));

	sectors[0]->addPortal(Portal(sectors[1], vec3(-384, 384, 1024), vec3(-128, 384, 1024), vec3(-384, 0, 1024)));
	sectors[1]->addPortal(Portal(sectors[0], vec3(-384, 384, 1024), vec3(-128, 384, 1024), vec3(-384, 0, 1024)));

	sectors[1]->addPortal(Portal(sectors[2], vec3(512, 384, 2816), vec3(512, 384, 3072), vec3(512, 0, 2816)));
	sectors[2]->addPortal(Portal(sectors[1], vec3(512, 384, 2816), vec3(512, 384, 3072), vec3(512, 0, 2816)));

	sectors[2]->addPortal(Portal(sectors[3], vec3(512, -256, 2304), vec3(512, -256, 2560), vec3(512, -640, 2304)));
	sectors[3]->addPortal(Portal(sectors[2], vec3(512, -256, 2304), vec3(512, -256, 2560), vec3(512, -640, 2304)));

	sectors[1]->addPortal(Portal(sectors[4], vec3(-1280, 384, 1664), vec3(-1280, 384, 1920), vec3(-1280, 128, 1664)));
	sectors[4]->addPortal(Portal(sectors[1], vec3(-1280, 384, 1664), vec3(-1280, 384, 1920), vec3(-1280, 128, 1664)));

	sectors[1]->addPortal(Portal(sectors[4], vec3(-1280,  192, 3840), vec3(-1280,  192, 4096), vec3(-1280, -256, 3840)));
	sectors[4]->addPortal(Portal(sectors[1], vec3(-1280,  192, 3840), vec3(-1280,  192, 4096), vec3(-1280, -256, 3840)));


	sectors[0]->addLight(vec3( 0,     128, 0   ), 800,  100, 100, 100);

	sectors[1]->addLight(vec3(-256,   224, 1800), 650,  100, 80,  100);
	sectors[1]->addLight(vec3(-512,   128, 3100), 900,  100, 100, 300);

	sectors[2]->addLight(vec3( 1300,  128, 2700), 800,  100, 100, 200);

	sectors[3]->addLight(vec3(-100,  -700, 2432), 600,  50,  50,  50);
	sectors[3]->addLight(vec3(-1450, -700, 2900), 1200, 250, 80,  250);

	sectors[4]->addLight(vec3(-2200,  256, 2300), 800,  100, 100, 100);
	sectors[4]->addLight(vec3(-2000,  0,   4000), 800,  100, 100, 100);

	return true;
}

bool MainApp::exit(){
	for (unsigned int i = 0; i < sectors.getCount(); i++){
		delete sectors[i];
	}

	return true;
}

bool MainApp::load(){
	bool err = false;

	if (!GL_ARB_shader_objects_supported || !GL_ARB_vertex_shader_supported || !GL_ARB_fragment_shader_supported || !GL_ARB_shading_language_100_supported){
		addToLog("No GLSL support (GL_ARB_shader_objects, GL_ARB_vertex_shader, GL_ARB_fragment_shader, GL_ARB_shading_language_100)\n");
		err = true;
	}
	if (!GL_ARB_occlusion_query_supported){
		addToLog("No occlusion query support (GL_ARB_occlusion_query)\n");
		err = true;
	}

	if (err) return false;

	setDefaultFont("../Textures/Fonts/Future.font", "../Textures/Fonts/Future.dds");

	if ((shader = renderer->addShader("shader.shd" )) == SHADER_NONE) return false;

	if ((base[0] = renderer->addTexture("../Textures/Wood.dds")) == TEXTURE_NONE) return false;
	if ((bump[0] = renderer->addTexture("../Textures/WoodBump.png", TEX_NORMALMAP)) == TEXTURE_NONE) return false;
	if ((base[1] = renderer->addTexture("../Textures/laying_rock7.dds")) == TEXTURE_NONE) return false;
	if ((bump[1] = renderer->addTexture("../Textures/laying_rock7Bump.png", TEX_NORMALMAP)) == TEXTURE_NONE) return false;
	if ((base[2] = renderer->addTexture("../Textures/victoria.dds")) == TEXTURE_NONE) return false;
	if ((bump[2] = renderer->addTexture("../Textures/victoriaBump.png", TEX_NORMALMAP)) == TEXTURE_NONE) return false;

	if ((particle = renderer->addTexture("../Textures/Particle.png")) == TEXTURE_NONE) return false;

	for (unsigned int i = 0; i < sectors.getCount(); i++){
		sectors[i]->init();
	}
	return true;
}

bool MainApp::unload(){
	for (unsigned int i = 0; i < sectors.getCount(); i++){
		sectors[i]->clear();
	}
	return true;
}

bool MainApp::drawFrame(){
	glMatrixMode(GL_PROJECTION);
	projection = projectionMatrixX(1.5f, float(height) / float(width), 0.1f, 6000);
	glLoadMatrixf(transpose(projection));

	modelView = rotateZXY(-wx, -wy, -wz);
	modelView.translate(-position);

    glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(transpose(modelView));

	renderer->changeMask(ALL);
	glClear(GL_DEPTH_BUFFER_BIT/* | GL_COLOR_BUFFER_BIT*/);

	glColor3f(1,1,1);


//	renderer->setShader(shader);
//	renderer->setTexture("Texture", tex0);
/*	renderer->apply();

	glBegin(GL_QUADS);
		glVertex3f(-1280,  192, 3840);
		glVertex3f(-1280,  192, 4096);
		glVertex3f(-1280, -256, 4096);
		glVertex3f(-1280, -256, 3840);
	glEnd();
*/
	unsigned int i, currSector = 0;
	float minDist = 1e10f;

	for (i = 0; i < sectors.getCount(); i++){
		sectors[i]->clearDrawFlag();

		// Works for this demo since all sectors have non-intersecting bounding boxes
		// Real large-scale applications would have to implement more sophisticated
		// ways to detect which sector the camera resides in.
		//if (sectors[i]->isInBoundingBox(position)) currSector = i;
		float d = sectors[i]->getDistanceSqr(position);
		if (d < minDist){
			currSector = i;
			minDist = d;
		}
	}

	vec3 dx(modelView.elem[0][0], modelView.elem[0][1], modelView.elem[0][2]);
	vec3 dy(modelView.elem[1][0], modelView.elem[1][1], modelView.elem[1][2]);
	sectors[currSector]->draw(renderer, shader, base, bump, position, dx, dy, useOcclusionQueries);

	for (i = 0; i < sectors.getCount(); i++){
		sectors[i]->drawParticles(renderer, particle);
	}

	return true;
}

App *app = new MainApp();
