/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// OpenGL|ES 2 demo using shader to compute mandelbrot/julia sets
// Thanks to Peter de Rivas for original Python code


#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>

#include <linux/input.h>

#include "bcm_host.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include <time.h>
clock_t begin = clock();

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

	GLuint resolution;
   GLuint verbose;
   GLuint vshader;
   GLuint fshader;
   GLuint mshader;
   GLuint program;
   GLuint program2;
   GLuint tex_fb;
   GLuint tex;
   GLuint buf;
// julia attribs
   GLuint unif_color, attr_vertex, unif_scale, unif_offset, unif_tex, unif_centre; 
// mandelbrot attribs
   GLuint attr_vertex2, unif_scale2, unif_offset2, unif_centre2;
   GLuint unif_time;
} CUBE_STATE_T;

static CUBE_STATE_T _state, *state=&_state;

#define check() assert(glGetError() == 0)

static void showlog(GLint shader)
{
   // Prints the compile log for a shader
   char log[1024];
   glGetShaderInfoLog(shader,sizeof log,NULL,log);
   printf("%d:shader:\n%s\n", shader, log);
}

static void showprogramlog(GLint shader)
{
   // Prints the information log for a program object
   char log[1024];
   glGetProgramInfoLog(shader,sizeof log,NULL,log);
   printf("%d:program:\n%s\n", shader, log);
}


    
/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(CUBE_STATE_T *state)
{
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   static const EGLint context_attributes[] = 
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   EGLConfig config;

   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);
   check();

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);
   check();

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);
   check();

   // get an appropriate EGL frame buffer configuration
   result = eglBindAPI(EGL_OPENGL_ES_API);
   assert(EGL_FALSE != result);
   check();

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, context_attributes);
   assert(state->context!=EGL_NO_CONTEXT);
   check();

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );
   
   glUniform2f(state->resolution, state->screen_width, state->screen_height);

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;
      
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;        

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );
         
   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/,  (DISPMANX_TRANSFORM_T)0/*transform*/);
      
   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
      
   check();

   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);
   check();

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);
   check();

   // Set background color and clear buffers
   glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
   glClear( GL_COLOR_BUFFER_BIT );

   check();
}

std::string readFile(const char *filePath) {

    std::string line = "";
    std::ifstream in(filePath);
    if(!in.is_open()) {
        std::cerr << "Could not read file " << filePath << ". File does not exist." << std::endl;
        return "";
    }
	std::string content((std::istreambuf_iterator<char>(in)), 
    std::istreambuf_iterator<char>());
    
    in.close();
    return content;
}



static void init_shaders(CUBE_STATE_T *state)
{
   static const GLfloat vertex_data[] = {
        -1.0,-1.0,1.0,1.0,
        1.0,-1.0,1.0,1.0,
        1.0,1.0,1.0,1.0,
        -1.0,1.0,1.0,1.0
   };
   
   //load up the shader files
   std::string vertShaderStr = readFile("vshader.vert");
    std::string fragShaderStr = readFile("myShader.frag"); 
    std::string juliaShaderStr = readFile("julia.frag"); 
    std::string mandelbrotShaderStr = readFile("mandelbrot.frag"); 
    const char *vertShaderSrc = vertShaderStr.c_str();
    const char *fragShaderSrc = fragShaderStr.c_str();
     const char *julia_fshader_source = juliaShaderStr.c_str();
     const char *mandelbrot_fshader_source = mandelbrotShaderStr.c_str();
   
   //basic vert shader
   const GLchar *vshader_source = vertShaderSrc;  
   //my custom fragment shader
   const GLchar *fshader_source = fragShaderSrc;
   
   //set verbose to always be true ( print out shader errors)
	state->verbose = true;
		
        state->vshader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(state->vshader, 1, &vshader_source, 0);
        glCompileShader(state->vshader);
        check();

        if (state->verbose)
            showlog(state->vshader);
            
        state->fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(state->fshader, 1, &julia_fshader_source, 0);
        glCompileShader(state->fshader);
        check();

        if (state->verbose)
            showlog(state->fshader);

        state->mshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(state->mshader, 1, &mandelbrot_fshader_source, 0);
        glCompileShader(state->mshader);
        check();

        if (state->verbose)
            showlog(state->mshader);

        // julia 
        state->program = glCreateProgram();
        glAttachShader(state->program, state->vshader);
        glAttachShader(state->program, state->fshader);
        glLinkProgram(state->program);
        check();

        if (state->verbose)
            showprogramlog(state->program);
            
        state->attr_vertex = glGetAttribLocation(state->program, "vertex");
        state->unif_color  = glGetUniformLocation(state->program, "color");
        state->unif_scale  = glGetUniformLocation(state->program, "scale");
        state->unif_offset = glGetUniformLocation(state->program, "offset");
        state->unif_tex    = glGetUniformLocation(state->program, "tex");       
        state->unif_centre = glGetUniformLocation(state->program, "centre");
        state->unif_time = glGetUniformLocation(state->program, "time");

        // mandelbrot
        state->program2 = glCreateProgram();
        glAttachShader(state->program2, state->vshader);
        glAttachShader(state->program2, state->mshader);
        glLinkProgram(state->program2);
        check();

        if (state->verbose)
            showprogramlog(state->program2);
            
        state->attr_vertex2 = glGetAttribLocation(state->program2, "vertex");
        state->unif_scale2  = glGetUniformLocation(state->program2, "scale");
        state->unif_offset2 = glGetUniformLocation(state->program2, "offset");
        state->unif_centre2 = glGetUniformLocation(state->program2, "centre");
        check();
   
        glClearColor ( 0.0, 1.0, 1.0, 1.0 );
        
        glGenBuffers(1, &state->buf);

        check();

        // Prepare a texture image
        glGenTextures(1, &state->tex);
        check();
        glBindTexture(GL_TEXTURE_2D,state->tex);
        check();
        // glActiveTexture(0)
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,state->screen_width,state->screen_height,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,0);
        check();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        check();
        // Prepare a framebuffer for rendering
        glGenFramebuffers(1,&state->tex_fb);
        check();
        glBindFramebuffer(GL_FRAMEBUFFER,state->tex_fb);
        check();
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,state->tex,0);
        check();
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        check();
        // Prepare viewport
        glViewport ( 0, 0, state->screen_width, state->screen_height );
        check();
        
        // Upload vertex data to a buffer
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data),
                             vertex_data, GL_STATIC_DRAW);
        glVertexAttribPointer(state->attr_vertex, 4, GL_FLOAT, 0, 16, 0);
        glEnableVertexAttribArray(state->attr_vertex);
        check();
}


static void draw_mandelbrot_to_texture(CUBE_STATE_T *state, GLfloat cx, GLfloat cy, GLfloat scale)
{
        // Draw the mandelbrot to a texture
        glBindFramebuffer(GL_FRAMEBUFFER,state->tex_fb);
        check();
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        
        glUseProgram ( state->program2 );
        check();

        glUniform2f(state->unif_scale2, scale, scale);
        glUniform2f(state->unif_centre2, cx, cy);
        check();
        glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
        check();
               
        glFlush();
        glFinish();
        check();
}
        
static void draw_triangles(CUBE_STATE_T *state, GLfloat cx, GLfloat cy, GLfloat scale, GLfloat x, GLfloat y)
{
        // Now render to the main frame buffer
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        // Clear the background (not really necessary I suppose)
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        check();
        
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        check();
        glUseProgram ( state->program );
        check();
        glBindTexture(GL_TEXTURE_2D,state->tex);
        check();
        glUniform4f(state->unif_color, 0.5, 0.5, 0.8, 1.0);
        glUniform2f(state->unif_scale, scale, scale);
        glUniform2f(state->unif_offset, x, y);
        glUniform2f(state->unif_centre, cx, cy);
        glUniform1i(state->unif_tex, 0); // I don't really understand this part, perhaps it relates to active texture?
        
        //pass time into the frag shader
        clock_t end = clock();
		double elapsed_secs = double(end - begin) / (CLOCKS_PER_SEC/100);
        glUniform1f(state->unif_time, elapsed_secs);
        check();
        
        glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
        check();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glFlush();
        glFinish();
        check();
        
        eglSwapBuffers(state->display, state->surface);
        check();
}

static int get_mouse(CUBE_STATE_T *state, int *outx, int *outy)
{
    static int fd = -1;
    const int width=state->screen_width, height=state->screen_height;
    static int x=800, y=400;
    const int XSIGN = 1<<4, YSIGN = 1<<5;
    if (fd<0) {
       fd = open("/dev/input/mouse0",O_RDONLY|O_NONBLOCK);
    }
    if (fd>=0) {
        struct {char buttons, dx, dy; } m;
        while (1) {
           int bytes = read(fd, &m, sizeof m);
           if (bytes < (int)sizeof m) goto _exit;
           if (m.buttons&8) {
              break; // This bit should always be set
           }
           read(fd, &m, 1); // Try to sync up again
        }
        if (m.buttons&3)
           return m.buttons&3;
        x+=m.dx;
        y+=m.dy;
        if (m.buttons&XSIGN)
           x-=256;
        if (m.buttons&YSIGN)
           y-=256;
        if (x<0) x=0;
        if (y<0) y=0;
        if (x>width) x=width;
        if (y>height) y=height;
   }
_exit:
   if (outx) *outx = x;
   if (outy) *outy = y;
   return 0;
} 


static const char *const evval[3] = {
    "RELEASED",
    "PRESSED ",
    "REPEATED"
};


int keyboardFd = -1;

bool setupKeyboard(){
	const char *dev = "/dev/input/by-id/usb-_USB_Keyboard-event-kbd";

    keyboardFd = open(dev, O_RDONLY|O_NONBLOCK);
    if (keyboardFd == -1) {
        fprintf(stderr, "Cannot open %s: %s.\n", dev, strerror(errno));
        return false;
    }
    return true;
}

bool readKeyboard(){
	struct input_event ev;
	ssize_t n;
	if(keyboardFd == -1){
		setupKeyboard();
	}
	
	n = read(keyboardFd, &ev, sizeof ev);
    if (n == (ssize_t)-1) {
				if (errno == EINTR)
			return true;
		else{
			//just continue here
			return true;
		}
	} else
	if (n != sizeof ev) {
		errno = EIO;
		
		return false;
	}
	if (ev.type == EV_KEY && ev.value >= 0 && ev.value <= 2)
		printf("%s 0x%04x (%d)\n", evval[ev.value], (int)ev.code, (int)ev.code);
		
	if (ev.code == KEY_Q){
		return false;
	}
	return true;
}      
 
//==============================================================================

int main ()
{
   int terminate = 0;
   GLfloat cx, cy;
   bcm_host_init();

   // Clear application state
   memset( state, 0, sizeof( *state ) );
      
   // Start OGLES
   init_ogl(state);
   init_shaders(state);
   cx = state->screen_width/2;
   cy = state->screen_height/2;
   
   setupKeyboard();

   //~ draw_mandelbrot_to_texture(state, cx, cy, 0.003);
   while (!terminate)
   {
      int x, y, b;
      //~ b = get_mouse(state, &x, &y);
      //~ if (b) break;
	  if(!readKeyboard()){
		break;
	  }
		
	draw_triangles(state, cx, cy, 0.003, x, y);

   }
   
   
   fflush(stdout);
    fprintf(stderr, "%s.\n", strerror(errno));
   
   return 0;
}

