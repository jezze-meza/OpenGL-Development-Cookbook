#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>

#ifdef __linux__
#include <GL/glx.h>
#include <X11/keysymdef.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GLSLShader.h"

#define GL_CHECK_ERRORS assert(glGetError()== GL_NO_ERROR);

#ifdef _WIN32
#ifdef _DEBUG 
#pragma comment(lib, "glew_static_x86_d.lib")
#pragma comment(lib, "freeglut_static_x86_d.lib")
#pragma comment(lib, "SOIL_static_x86_d.lib")
#else
#pragma comment(lib, "glew_static_x86.lib")
#pragma comment(lib, "freeglut_static_x86.lib")
#pragma comment(lib, "SOIL_static_x86.lib")
#endif
#endif


using namespace std;

//screen size
const int WIDTH  = 1280;
const int HEIGHT = 960;

//for floating point imprecision
const float EPSILON = 0.001f;
const float EPSILON2 = EPSILON*EPSILON;

//camera tranformation variables
int state = 0, oldX=0, oldY=0;
float rX=0, rY=0, fov = 45;

#include "FreeCamera.h"

#ifdef _WIN32
//virtual key codes
const int VK_W = 0x57;
const int VK_S = 0x53;
const int VK_A = 0x41;
const int VK_D = 0x44;
const int VK_Q = 0x51;
const int VK_Z = 0x5a;
#else

const KeySym VK_W = XK_w;
const KeySym VK_S = XK_s;
const KeySym VK_A = XK_a;
const KeySym VK_D = XK_d;
const KeySym VK_Q = XK_q;
const KeySym VK_Z = XK_z;

#endif

//delta time
float dt = 0;

//time related variables
float current_time=0, last_time=0;

//free camera instance
CFreeCamera cam;

//grid object
#include "Grid.h"
CGrid* grid;

//unit cube object
#include "UnitCube.h"
CUnitCube* cube;

//modelview and projection matrices
glm::mat4 MV,P;

//selected box index
int selected_box=-1;

//box positions
glm::vec3 box_positions[3]={glm::vec3(-1,0.5,0),
							glm::vec3(0,0.5,1),
							glm::vec3(1,0.5,0)
							};


//box struct 
struct Box { glm::vec3 min, max;}boxes[3];
#include <limits>

//simple Ray struct
struct Ray {

public:
	glm::vec3 origin, direction;
	float t;

	Ray() {
		t=std::numeric_limits<float>::max();
		origin=glm::vec3(0);
		direction=glm::vec3(0);
	}
}eyeRay;

//ray Box intersection code
glm::vec2 intersectBox(const Ray& ray, const Box& cube) {
	glm::vec3 inv_dir = 1.0f/ray.direction;
	glm::vec3   tMin = (cube.min - ray.origin) * inv_dir;
	glm::vec3   tMax = (cube.max - ray.origin) * inv_dir;
	glm::vec3     t1 = glm::min(tMin, tMax);
	glm::vec3     t2 = glm::max(tMin, tMax);
	float tNear = max(max(t1.x, t1.y), t1.z);
	float  tFar = min(min(t2.x, t2.y), t2.z);
	return glm::vec2(tNear, tFar);
}

//output message
#include <sstream>
std::stringstream msg;

//mouse filtering variables
const float MOUSE_FILTER_WEIGHT=0.75f;
const int MOUSE_HISTORY_BUFFER_SIZE = 10;

//mouse history buffer
glm::vec2 mouseHistory[MOUSE_HISTORY_BUFFER_SIZE];

float mouseX=0, mouseY=0; //filtered mouse values

//flag to enable filtering
bool useFiltering = true;

//mouse move filtering function
void filterMouseMoves(float dx, float dy) {
    for (int i = MOUSE_HISTORY_BUFFER_SIZE - 1; i > 0; --i) {
        mouseHistory[i] = mouseHistory[i - 1];
    }

    // Store current mouse entry at front of array.
    mouseHistory[0] = glm::vec2(dx, dy);

    float averageX = 0.0f;
    float averageY = 0.0f;
    float averageTotal = 0.0f;
    float currentWeight = 1.0f;

    // Filter the mouse.
    for (int i = 0; i < MOUSE_HISTORY_BUFFER_SIZE; ++i)
    {
		glm::vec2 tmp=mouseHistory[i];
        averageX += tmp.x * currentWeight;
        averageY += tmp.y * currentWeight;
        averageTotal += 1.0f * currentWeight;
        currentWeight *= MOUSE_FILTER_WEIGHT;
    }

    mouseX = averageX / averageTotal;
    mouseY = averageY / averageTotal;

}

//mouse click handler
void OnMouseDown(int button, int s, int x, int y)
{
	if (s == GLUT_DOWN)
	{
		oldX = x;
		oldY = y;

		//unproject the ray at the click position at two different depths 0 at near clip plne
		//and 1 at far clip plane. These give us two world space points. Subtracting these we get 
		//the ray direction vector.
		glm::vec3 start = glm::unProject(glm::vec3(x,HEIGHT-y,0), MV, P, glm::vec4(0,0,WIDTH,HEIGHT));
		glm::vec3   end = glm::unProject(glm::vec3(x,HEIGHT-y,1), MV, P, glm::vec4(0,0,WIDTH,HEIGHT));

		eyeRay.origin=cam.GetPosition();
		eyeRay.direction =  glm::normalize(end-start);
		float tMin = numeric_limits<float>::max();
		selected_box = -1;

		//next we loop through all scene objects and find the ray intersection with the bounding box
		//of each object. The nearest intersected object's index is stored
		for(int i=0;i<3;i++) {
			glm::vec2 tMinMax = intersectBox(eyeRay, boxes[i]);
			if(tMinMax.x<tMinMax.y && tMinMax.x<tMin) {
				selected_box=i;
				tMin = tMinMax.x;
			}
		}
		if(selected_box==-1)
			cout<<"No box picked"<<endl;
		else
			cout<<"Selected box: "<<selected_box<<endl;
	}

	if(button == GLUT_MIDDLE_BUTTON)
		state = 0;
	else
		state = 1;
}


//mouse move handler
void OnMouseMove(int x, int y)
{
	if(selected_box == -1) {
		if (state == 0) {
			fov += (y - oldY)/5.0f;
			cam.SetupProjection(fov, cam.GetAspectRatio());
		} else {
			rY += (y - oldY)/5.0f;
			rX += (oldX-x)/5.0f;

			if(useFiltering)
				filterMouseMoves(rX, rY);
			else {
				mouseX = rX;
				mouseY = rY;
			}
			cam.Rotate(mouseX,mouseY, 0);
		}
		oldX = x;
		oldY = y;
	}
	glutPostRedisplay();
}

//OpenGL initialization
void OnInit() {

	GL_CHECK_ERRORS

	//create a grid of size 20x20 in XZ plane
	grid = new CGrid(20,20);

	//create a unit cube
	cube = new CUnitCube();

	GL_CHECK_ERRORS

	//set the camera position
	glm::vec3 p = glm::vec3(10,10,10);
	cam.SetPosition(p);

	//get the camera look direction to obtain the yaw and pitch values for camera rotation	
	glm::vec3 look=  glm::normalize(p);

	float yaw = glm::degrees(float(atan2(look.z, look.x)+M_PI));
	float pitch = glm::degrees(asin(look.y));
	rX = yaw;
	rY = pitch;

	//if filtering is enabled, save positions to mouse history buffer
	if(useFiltering) {
		for (int i = 0; i < MOUSE_HISTORY_BUFFER_SIZE ; ++i) {
			mouseHistory[i] = glm::vec2(rX, rY);
		}
	}
	cam.Rotate(rX,rY,0);

	//enable depth testing
	glEnable(GL_DEPTH_TEST);

	//determine the scene geometry bounding boxes
	for(int i=0;i<3;i++) {
		boxes[i].min=box_positions[i]-0.5f;
		boxes[i].max=box_positions[i]+0.5f;
	}

	cout<<"Initialization successfull"<<endl;
}

//release all allocated resources
void OnShutdown() {

	delete grid;
	delete cube;
	cout<<"Shutdown successfull"<<endl;
}

//resize event handler
void OnResize(int w, int h) {
	//set the viewport size
	glViewport (0, 0, (GLsizei) w, (GLsizei) h);
	//set the camera projection
	cam.SetupProjection(fov, (GLfloat)w/h);
}

#ifdef __linux__
bool GetAsyncKeyState(KeySym sym) {
    Display *display = glXGetCurrentDisplay();

    KeyCode code = XKeysymToKeycode(display, sym);
    if (code == NoSymbol) return false;
    static char key_vector[32];
    XQueryKeymap(display, key_vector);
    return (key_vector[code/8] >> (code&7)) & 1;
}
#endif

//set the idle callback
void OnIdle() {

	//handle the WSAD, QZ key events to move the camera around
#ifdef _WIN32
	if( GetAsyncKeyState(VK_W) & 0x8000) {
		cam.Walk(dt);
	}

	if( GetAsyncKeyState(VK_S) & 0x8000) {
		cam.Walk(-dt);
	}

	if( GetAsyncKeyState(VK_A) & 0x8000) {
		cam.Strafe(-dt);
	}

	if( GetAsyncKeyState(VK_D) & 0x8000) {
		cam.Strafe(dt);
	}

	if( GetAsyncKeyState(VK_Q) & 0x8000) {
		cam.Lift(dt);
	}

	if( GetAsyncKeyState(VK_Z) & 0x8000) {
		cam.Lift(-dt);
	}
#else
	if( GetAsyncKeyState(VK_W) ) {
		cam.Walk(dt);
	}

	if( GetAsyncKeyState(VK_S) ) {
		cam.Walk(-dt);
	}

	if( GetAsyncKeyState(VK_A) ) {
		cam.Strafe(-dt);
	}

	if( GetAsyncKeyState(VK_D) ) {
		cam.Strafe(dt);
	}

	if( GetAsyncKeyState(VK_Q) ) {
		cam.Lift(dt);
	}

	if( GetAsyncKeyState(VK_Z) ) {
		cam.Lift(-dt);
	}
#endif

	glm::vec3 t = cam.GetTranslation();
	if(glm::dot(t,t)>EPSILON2) {
		cam.SetTranslation(t*0.95f);
	}

	//call the display function
	glutPostRedisplay();
}

//display callback function
void OnRender() {
	
	//timing related calcualtion
	last_time = current_time;
	current_time = glutGet(GLUT_ELAPSED_TIME)/1000.0f;
	dt = current_time-last_time;

	//clear colour and depth buffers
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	//set the mesage
	msg.str(std::string());
	if(selected_box==-1)
		msg<<"No box picked";
	else
		msg<<"Picked box: "<<selected_box;

	//set the window title
	glutSetWindowTitle(msg.str().c_str());

	//from the camera modelview and projection matrices get the combined MVP matrix
	MV	= cam.GetViewMatrix();
	P   = cam.GetProjectionMatrix();
    glm::mat4 MVP	= P*MV;

	//render the grid object
	grid->Render(glm::value_ptr(MVP));

	//set the first cube transform 
	//set its colour to cyan if selected, red otherwise
	glm::mat4 T = glm::translate(glm::mat4(1), box_positions[0]);
	cube->color = (selected_box==0)?glm::vec3(0,1,1):glm::vec3(1,0,0);
	cube->Render(glm::value_ptr(MVP*T));

	//set the second cube transform 
	//set its colour to cyan if selected, green otherwise
	T = glm::translate(glm::mat4(1), box_positions[1]);
	cube->color = (selected_box==1)?glm::vec3(0,1,1):glm::vec3(0,1,0);
	cube->Render(glm::value_ptr(MVP*T));

	//set the third cube transform 
	//set its colour to cyan if selected, blue otherwise
	T = glm::translate(glm::mat4(1), box_positions[2]);
	cube->color = (selected_box==2)?glm::vec3(0,1,1):glm::vec3(0,0,1);
	cube->Render(glm::value_ptr(MVP*T));

	//swap front and back buffers to show the rendered result
	glutSwapBuffers();
}

int main(int argc, char** argv) {
	//freeglut initialization calls
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitContextVersion (3, 3);
	glutInitContextFlags (GLUT_CORE_PROFILE | GLUT_DEBUG);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("Picking using scene intersection queries - OpenGL 3.3");

	//glew initialization
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)	{
		cerr<<"Error: "<<glewGetErrorString(err)<<endl;
	} else {
		if (GLEW_VERSION_3_3)
		{
			cout<<"Driver supports OpenGL 3.3\nDetails:"<<endl;
		}
	}
	err = glGetError(); //this is to ignore INVALID ENUM error 1282
	GL_CHECK_ERRORS

	//print information on screen
	cout<<"\tUsing GLEW "<<glewGetString(GLEW_VERSION)<<endl;
	cout<<"\tVendor: "<<glGetString (GL_VENDOR)<<endl;
	cout<<"\tRenderer: "<<glGetString (GL_RENDERER)<<endl;
	cout<<"\tVersion: "<<glGetString (GL_VERSION)<<endl;
	cout<<"\tGLSL: "<<glGetString (GL_SHADING_LANGUAGE_VERSION)<<endl;

	GL_CHECK_ERRORS

	//opengl initialization
	OnInit();

	//callback hooks
	glutCloseFunc(OnShutdown);
	glutDisplayFunc(OnRender);
	glutReshapeFunc(OnResize);
	glutMouseFunc(OnMouseDown);
	glutMotionFunc(OnMouseMove);
	glutIdleFunc(OnIdle);

	//call main loop
	glutMainLoop();
	
	return 0;
}
