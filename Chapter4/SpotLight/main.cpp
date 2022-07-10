#include <GL/glew.h>

#define _USE_MATH_DEFINES
#include <cmath>

#include <GL/freeglut.h>
#include <iostream>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "../src/GLSLShader.h"

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
const int WIDTH  = 1024;
const int HEIGHT = 768;

//per-fragment spoot light shader 
GLSLShader shader;
GLSLShader* pFlatShader; // we will reuse the grid shader for crosshair at light position

//vertex struct with position and normal
struct Vertex {
	glm::vec3 pos, normal;
};

//sphere vertex array and vertex buffer object IDs
GLuint sphereVAOID;
GLuint sphereVerticesVBO;
GLuint sphereIndicesVBO;

//cube vertex array and vertex buffer object IDs
GLuint cubeVAOID;
GLuint cubeVerticesVBO;
GLuint cubeIndicesVBO;

//light crosshair gizmo vertex array and vertex buffer objects
GLuint lightVAOID;
GLuint lightVerticesVBO;

//projection, modelview matrices
glm::mat4  P = glm::mat4(1);
glm::mat4  MV = glm::mat4(1); 

//camera transformation variables
int state = 0, oldX=0, oldY=0;
float rX=25, rY=-40, dist = -10;

//Grid object
#include "../src/Grid.h"
CGrid* grid;

//spot light properties
glm::vec3 lightPosOS=glm::vec3(0,2,0); //objectspace light position
glm::vec3 lightPosES;					//eyespace light position
glm::vec3 spotPositionOS=glm::vec3(0,1,0);	//objectspace spot position
glm::vec3 spotDirectionES;					//eyespace spot direction
float spot_cutoff = cos(glm::radians(45.0f));
float spot_exponent = 1;
 
#include <vector>

//vertices and indices for sphere/cube
std::vector<Vertex> vertices;
std::vector<GLushort> indices;
int totalSphereTriangles = 0;

//spherical coordinates for storing the light position
float theta = -10;
float phi = 0.22f;
float radius = 2.5;

//adds the given sphere indices to the indices vector
inline void push_indices(int sectors, int r, int s, std::vector<GLushort>& indices) {
    int curRow = r * sectors;
    int nextRow = (r+1) * sectors;

    indices.push_back(curRow + s);
    indices.push_back(nextRow + s);
    indices.push_back(nextRow + (s+1));

    indices.push_back(curRow + s);
    indices.push_back(nextRow + (s+1));
    indices.push_back(curRow + (s+1));
}

//generates a sphere primitive with the given radius, slices and stacks
void CreateSphere( float radius, unsigned int slices, unsigned int stacks, std::vector<Vertex>& vertices, std::vector<GLushort>& indices)
{
    float const R = 1.0f/(float)(slices-1);
    float const S = 1.0f/(float)(stacks-1);

    for(size_t r = 0; r < slices; ++r) {
        for(size_t s = 0; s < stacks; ++s) {
            float const y = (float)(sin( -M_PI_2 + M_PI * r * R ));
            float const x = (float)(cos(2*M_PI * s * S) * sin( M_PI * r * R ));
            float const z = (float)(sin(2*M_PI * s * S) * sin( M_PI * r * R ));

			Vertex v;
			v.pos = glm::vec3(x,y,z)*radius;
			v.normal = glm::normalize(v.pos);
            vertices.push_back(v);
            push_indices(stacks, r, s, indices);
        }
    }

}

//generates a cube of the given size
void CreateCube(const float& size, std::vector<Vertex>& vertices, std::vector<GLushort>& indices) {
	float halfSize = size/2.0f;
	glm::vec3 positions[8];
	positions[0]=glm::vec3(-halfSize,-halfSize,-halfSize);
	positions[1]=glm::vec3( halfSize,-halfSize,-halfSize);
	positions[2]=glm::vec3( halfSize, halfSize,-halfSize);
	positions[3]=glm::vec3(-halfSize, halfSize,-halfSize);
	positions[4]=glm::vec3(-halfSize,-halfSize, halfSize);
	positions[5]=glm::vec3( halfSize,-halfSize, halfSize);
	positions[6]=glm::vec3( halfSize, halfSize, halfSize);
	positions[7]=glm::vec3(-halfSize, halfSize, halfSize);

	glm::vec3 normals[6];
	normals[0]=glm::vec3(-1.0,0.0,0.0);
	normals[1]=glm::vec3(1.0,0.0,0.0);
	normals[2]=glm::vec3(0.0,1.0,0.0);
	normals[3]=glm::vec3(0.0,-1.0,0.0);
	normals[4]=glm::vec3(0.0,0.0,1.0);
	normals[5]=glm::vec3(0.0,0.0,-1.0);

	indices.resize(36);
	vertices.resize(36);

	//fill indices array
	GLushort* id=&indices[0];
	//left face
	*id++ = 7; 	*id++ = 3; 	*id++ = 4;
	*id++ = 3; 	*id++ = 0; 	*id++ = 4;

	//right face
	*id++ = 2; 	*id++ = 6; 	*id++ = 1;
	*id++ = 6; 	*id++ = 5; 	*id++ = 1;

	//top face
	*id++ = 7; 	*id++ = 6; 	*id++ = 3;
	*id++ = 6; 	*id++ = 2; 	*id++ = 3;
	//bottom face
	*id++ = 0; 	*id++ = 1; 	*id++ = 4;
	*id++ = 1; 	*id++ = 5; 	*id++ = 4;

	//front face
	*id++ = 6; 	*id++ = 4; 	*id++ = 5;
	*id++ = 6; 	*id++ = 7; 	*id++ = 4;
	//back face
	*id++ = 0; 	*id++ = 2; 	*id++ = 1;
	*id++ = 0; 	*id++ = 3; 	*id++ = 2;


	for(int i=0;i<36;i++) {
		int normal_index = i/6;
		vertices[i].pos=positions[indices[i]];
		vertices[i].normal=normals[normal_index];
		indices[i]=i;
	}
}

//mouse click handler
void OnMouseDown(int button, int s, int x, int y)
{
	if (s == GLUT_DOWN)
	{
		oldX = x;
		oldY = y;
	}

	if(button == GLUT_MIDDLE_BUTTON)
		state = 0;
	else if(button == GLUT_RIGHT_BUTTON)
		state = 2;
	else
		state = 1;
}

//mouse move handler
void OnMouseMove(int x, int y)
{
	if (state == 0)
		dist *= (1 + (y - oldY)/60.0f);
	else if(state ==2) {
		theta += (oldX - x)/60.0f;
		phi += (y - oldY)/60.0f;

		lightPosOS.x = spotPositionOS.x + radius * cos(theta)*sin(phi);
		lightPosOS.y = spotPositionOS.y + radius * cos(phi);
		lightPosOS.z = spotPositionOS.z + radius * sin(theta)*sin(phi);

	} else {
		rY += (x - oldX)/5.0f;
		rX += (y - oldY)/5.0f;
	}
	oldX = x;
	oldY = y;

	glutPostRedisplay();
}

//OpenGL initialization
void OnInit() {
	 
	//load the per-fragment spot light shader
	shader.LoadFromFile(GL_VERTEX_SHADER, "shaders/SpotLight.vert");
	shader.LoadFromFile(GL_FRAGMENT_SHADER, "shaders/SpotLight.frag");
	//compile and link shader
	shader.CreateAndLinkProgram();
	shader.Use();
		//add attributes and uniforms
		shader.AddAttribute("vVertex");
		shader.AddAttribute("vNormal");
		shader.AddUniform("MVP");
		shader.AddUniform("MV");
		shader.AddUniform("N");
		shader.AddUniform("light_position");
		shader.AddUniform("spot_direction");
		shader.AddUniform("spot_cutoff");
		shader.AddUniform("spot_exponent");
		shader.AddUniform("diffuse_color");
		//pass values of constant uniforms at initialization
		glUniform1f(shader("spot_cutoff"), spot_cutoff);
		glUniform1f(shader("spot_exponent"), spot_exponent);
	shader.UnUse();

	GL_CHECK_ERRORS

	//setup sphere geometry
	CreateSphere(1.0f,10,10, vertices, indices);

	//setup sphere vao and vbo stuff
	glGenVertexArrays(1, &sphereVAOID);
	glGenBuffers(1, &sphereVerticesVBO);
	glGenBuffers(1, &sphereIndicesVBO);
	glBindVertexArray(sphereVAOID);

		glBindBuffer (GL_ARRAY_BUFFER, sphereVerticesVBO);
		//pass vertices to the buffer object
		glBufferData (GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
		GL_CHECK_ERRORS
		//enable vertex attribute array for position
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,sizeof(Vertex),0);
		GL_CHECK_ERRORS
		//enable vertex attribute array for normal
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,sizeof(Vertex), (const GLvoid*)(offsetof(Vertex, normal)));
		GL_CHECK_ERRORS
		//pass sphere indices to element array buffer 
		glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sphereIndicesVBO);
		glBufferData (GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(GLushort), &indices[0], GL_STATIC_DRAW);

	//store the total number of sphere triangles
	totalSphereTriangles = indices.size();

	//clear the vertices and indices vectors as we will reuse them
	//for cubes
	vertices.clear();
	indices.clear();

	//setup cube geometry
	CreateCube(2,vertices, indices);

	//setup cube vao and vbo stuff
	glGenVertexArrays(1, &cubeVAOID);
	glGenBuffers(1, &cubeVerticesVBO);
	glGenBuffers(1, &cubeIndicesVBO);
	glBindVertexArray(cubeVAOID);

		glBindBuffer (GL_ARRAY_BUFFER, cubeVerticesVBO);
		//pass vertices to the buffer object
		glBufferData (GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
		GL_CHECK_ERRORS
		//enable vertex attribute array for position
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,sizeof(Vertex),0);
		GL_CHECK_ERRORS
		
		//enable vertex attribute array for normal
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,sizeof(Vertex), (const GLvoid*)(offsetof(Vertex, normal)));
		GL_CHECK_ERRORS

		//pass cube indices to element array buffer 
		glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, cubeIndicesVBO);
		glBufferData (GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(GLushort), &indices[0], GL_STATIC_DRAW);

	GL_CHECK_ERRORS

	//setup vao and vbo stuff for the light position crosshair
	glm::vec3 crossHairVertices[6];
	crossHairVertices[0] = glm::vec3(-0.5f,0,0);
	crossHairVertices[1] = glm::vec3(0.5f,0,0);
	crossHairVertices[2] = glm::vec3(0, -0.5f,0);
	crossHairVertices[3] = glm::vec3(0, 0.5f,0);
	crossHairVertices[4] = glm::vec3(0,0, -0.5f);
	crossHairVertices[5] = glm::vec3(0,0, 0.5f);

	//setup light gizmo vertex array and buffer object
	glGenVertexArrays(1, &lightVAOID);
	glGenBuffers(1, &lightVerticesVBO);
	glBindVertexArray(lightVAOID);
		//pass light crosshair gizmo vertices to buffer object
		glBindBuffer (GL_ARRAY_BUFFER, lightVerticesVBO);
		glBufferData (GL_ARRAY_BUFFER, sizeof(crossHairVertices), &(crossHairVertices[0].x), GL_STATIC_DRAW);
		GL_CHECK_ERRORS
		//enable vertex attribute array for position
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,0);

		GL_CHECK_ERRORS

	//enable depth testing and culling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	
	GL_CHECK_ERRORS

	//create a grid of 10x10 size in XZ plane
	grid = new CGrid();

	GL_CHECK_ERRORS

	//get the grid shader for rendering of the light's crosshair gizmo
	pFlatShader = grid->GetShader();

	//get the light position using the spherical coordinates
	lightPosOS.x = spotPositionOS.x + radius * cos(theta)*sin(phi);
	lightPosOS.y = spotPositionOS.y + radius * cos(phi);
	lightPosOS.z = spotPositionOS.z + radius * sin(theta)*sin(phi);

	cout<<"Initialization successfull"<<endl;
}

//release all allocated resources
void OnShutdown() {
	pFlatShader = NULL;

	//Destroy shader
	shader.DeleteShaderProgram();
	//Destroy vao and vbo
	glDeleteBuffers(1, &sphereVerticesVBO);
	glDeleteBuffers(1, &sphereIndicesVBO);
	glDeleteVertexArrays(1, &sphereVAOID);

	glDeleteBuffers(1, &cubeVerticesVBO);
	glDeleteBuffers(1, &cubeIndicesVBO);
	glDeleteVertexArrays(1, &cubeVAOID);

	glDeleteVertexArrays(1, &lightVAOID);
	glDeleteBuffers(1, &lightVerticesVBO);

	delete grid;
	cout<<"Shutdown successfull"<<endl;
}

//resize event handler
void OnResize(int w, int h) {
	//set the viewport
	glViewport (0, 0, (GLsizei) w, (GLsizei) h);
	//setup the projection matrix
	P = glm::perspective(glm::radians(45.0f), (GLfloat)w/h, 0.1f, 1000.f);
}

//idle callback just calls the display function
void OnIdle() {
	glutPostRedisplay();
}

//scene rendering function
void DrawScene(const glm::mat4& MView, const glm::mat4& Proj ) {

	GL_CHECK_ERRORS

	//bind the current shader
	shader.Use();

	//bind the cube vertex array object
	glBindVertexArray(cubeVAOID); {
		//set the cube's transform
		glm::mat4 T = glm::translate(glm::mat4(1), glm::vec3(-1,1,0));
		glm::mat4 cM = T;				 //Model matrix
		glm::mat4 cMV = MView*cM;		 //ModelView matrix
		glm::mat4 cMVP = Proj*cMV;		 //combined ModelView  Projection matrix

		//pass shader uniforms
		glUniformMatrix4fv(shader("MVP"), 1, GL_FALSE, glm::value_ptr(cMVP));
		glUniformMatrix4fv(shader("MV"), 1, GL_FALSE, glm::value_ptr(cMV));
		glUniformMatrix3fv(shader("N"), 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(cMV))));
		glUniform3f(shader("diffuse_color"), 1.0f,0.0f,0.0f);
		glUniform3fv(shader("light_position"),1, &(lightPosES.x));
		glUniform3fv(shader("spot_direction"),1, &(spotDirectionES.x));
			//draw triangles
	 		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
	}
	//bind the sphere vertex array object
	glBindVertexArray(sphereVAOID); {
		//set the sphere's transform
		glm::mat4 T = glm::translate(glm::mat4(1), glm::vec3(1,1,0));
		glm::mat4 cM = T;
		glm::mat4 cMV = MView*cM;
		glm::mat4 cMVP = Proj*cMV;
		//pass shader uniforms
		glUniformMatrix4fv(shader("MVP"), 1, GL_FALSE, glm::value_ptr(cMVP));
		glUniformMatrix4fv(shader("MV"), 1, GL_FALSE, glm::value_ptr(cMV));
		glUniformMatrix3fv(shader("N"), 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(cMV))));
		glUniform3f(shader("diffuse_color"), 0.0f, 0.0f, 1.0f);
			//draw triangles
		 	glDrawElements(GL_TRIANGLES, totalSphereTriangles, GL_UNSIGNED_SHORT, 0);
	}
	//unbind shader
	shader.UnUse();

	GL_CHECK_ERRORS

	//bind light gizmo vertex array object
	glBindVertexArray(lightVAOID); {
		//set light's transform
		glm::mat4 T = glm::translate(glm::mat4(1), lightPosOS);
		//bind shader and draw 3 lines
		pFlatShader->Use();
			glUniformMatrix4fv((*pFlatShader)("MVP"), 1, GL_FALSE, glm::value_ptr(Proj*MView*T));
				glDrawArrays(GL_LINES, 0, 6);
		//unbind shader
		pFlatShader->UnUse();
	}
	//unbind the vertex array object
	glBindVertexArray(0);
	//render grid object
	grid->Render(glm::value_ptr(Proj*MView));
}

//display callback function
void OnRender() {

	//clear colour and depth buffers
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	//set the camera transform
	glm::mat4 T		= glm::translate(glm::mat4(1.0f),glm::vec3(0.0f, 0.0f, dist));
	glm::mat4 Rx	= glm::rotate(T,  rX, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 MV    = glm::rotate(Rx, rY, glm::vec3(0.0f, 1.0f, 0.0f));

	//get the light and spot direction in eye space by multiplying with the
	//modelview matrix
	lightPosES = glm::vec3(MV*glm::vec4(lightPosOS,1));
	spotDirectionES  = glm::normalize(glm::vec3(MV*glm::vec4(spotPositionOS-lightPosOS,0)));

	//render scene
	DrawScene(MV, P);

	//swap front and back buffers to show the rendered result
	glutSwapBuffers();
}

//mouse wheel scroll handler which changes the radius of the light source
//since the position is given in spherical coordinates, the radius contols 
//how far from the center the light source is.
void OnMouseWheel(int button, int dir, int x, int y) {

	if (dir > 0)
    {
        radius += 0.1f;
    }
    else
    {
        radius -= 0.1f;
    }

	radius = max(radius,0.0f);

	//get the new light position
	lightPosOS.x = spotPositionOS.x + radius * cos(theta)*sin(phi);
	lightPosOS.y = spotPositionOS.y + radius * cos(phi);
	lightPosOS.z = spotPositionOS.z + radius * sin(theta)*sin(phi);

	//call the display function
	glutPostRedisplay();
}

int main(int argc, char** argv) {
	//freeglut initialization calls
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitContextVersion (3, 3);
	glutInitContextFlags (GLUT_CORE_PROFILE | GLUT_DEBUG);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("Spot Light - OpenGL 3.3");

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

	cout<<"\tUsing GLEW "<<glewGetString(GLEW_VERSION)<<endl;
	cout<<"\tVendor: "<<glGetString (GL_VENDOR)<<endl;
	cout<<"\tRenderer: "<<glGetString (GL_RENDERER)<<endl;
	cout<<"\tVersion: "<<glGetString (GL_VERSION)<<endl;
	cout<<"\tGLSL: "<<glGetString (GL_SHADING_LANGUAGE_VERSION)<<endl;

	GL_CHECK_ERRORS

	//print information on screen
	OnInit();

	//initialization of OpenGL
	glutCloseFunc(OnShutdown);
	glutDisplayFunc(OnRender);
	glutReshapeFunc(OnResize);
	glutMouseFunc(OnMouseDown);
	glutMotionFunc(OnMouseMove);
	glutMouseWheelFunc(OnMouseWheel);
	glutIdleFunc(OnIdle);

	//main loop call
	glutMainLoop();
	return 0;
}