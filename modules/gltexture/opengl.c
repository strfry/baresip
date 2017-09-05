/**
 * @file opengl.c Video driver for OpenGL on MacOSX
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */

#define GL_GLEXT_PROTOTYPES
#define _XOPEN_SOURCE 500

#ifdef RASPBERRY_PI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "bcm_host.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#else

#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

#endif

#include <re.h>
#include <rem.h>
#include <baresip.h>


/**
 * @defgroup opengl opengl
 *
 * Video display module for OpenGL (ES) 2.0 texture with external context
 */

struct vidisp_st {
	const struct vidisp *vd;        /**< Inheritance (1st)     */
	struct vidsz size;              /**< Current size          */
	int PHandle;
	char *prog;
//	SDL_Window* window;
};


static struct vidisp *vid;       /**< OPENGL Video-display      */

static const char* VProgram =
	"attribute vec2 position;    \n"
	"attribute vec2 texcoord_in;    \n"
	"varying vec2 texcoord;"
	"void main()                  \n"
	"{                            \n"
	"   texcoord = texcoord_in;           \n"
	"   gl_Position = vec4(position, 0.0, 1.0);  \n"
	"}                            \n";

static const char *FProgram=
  "uniform sampler2D Ytex;\n"
  "uniform sampler2D Utex,Vtex;\n"
  "varying vec2 texcoord; \n"
  "void main(void) {\n"
  "  float nx,ny,r,g,b,y,u,v;\n"
  "  vec4 txl,ux,vx;          \n"
  "	 float width = %d.0;      \n"
  "	 float height = %d.0;      \n"
  "  y=texture2D(Ytex,texcoord).r;\n"
  "  u=texture2D(Utex,texcoord).r;\n"
  "  v=texture2D(Vtex,texcoord).r;\n"

  "  y=1.1643*(y-0.0625);\n"
  "  u=u-0.5;\n"
  "  v=v-0.5;\n"

  "  r=y+1.5958*v;\n"
  "  g=y-0.39173*u-0.81290*v;\n"
  "  b=y+2.017*u;\n"

  "  gl_FragColor=vec4(r,g,b,1.0);\n"
  "}\n";

//  "  nx=gl_FragCoord.x / width;  \n"
//  "  ny=(height - gl_FragCoord.y) / height;\n"


static void destructor(void *arg)
{
	struct vidisp_st *st = arg;

	if (st->PHandle) {
		glUseProgram(0);
		glDeleteProgram(st->PHandle);
	}

	mem_deref(st->prog);
}


static int create_window(struct vidisp_st *st)
{
	/*NSRect rect = NSMakeRect(0, 0, 100, 100);
	NSUInteger style;*/

	//if (st->win)
	//	return 0;

/*
	style = NSTitledWindowMask |
		NSClosableWindowMask |
		NSMiniaturizableWindowMask;

	st->win = [[NSWindow alloc] initWithContentRect:rect
				    styleMask:style
				    backing:NSBackingStoreBuffered
				    defer:FALSE];
	if (!st->win) {
		warning("opengl: could not create NSWindow\n");
		return ENOMEM;
	}

	[st->win setLevel:NSFloatingWindowLevel];
*/
	return 0;
}


static void opengl_reset(struct vidisp_st *st, const struct vidsz *sz)
{
	if (st->PHandle) {
		glUseProgram(0);
		glDeleteProgram(st->PHandle);
		st->PHandle = 0;
		st->prog = mem_deref(st->prog);
	}

	st->size = *sz;
}


static int setup_shader(struct vidisp_st *st, int width, int height)
{
	GLuint VSHandle, FSHandle, PHandle;
	const char *progv[1];
	char buf[1024];
	int err, i;

	if (st->PHandle)
		return 0;

	err = re_sdprintf(&st->prog, FProgram, width, height);
	if (err)
		return err;

	glClearColor(0, 0, 0, 0);
	//glColor3f(1.0f, 0.84f, 0.0f);
	
	/* Set up program objects. */
	PHandle = glCreateProgram();
	VSHandle = glCreateShader(GL_VERTEX_SHADER);
	FSHandle = glCreateShader(GL_FRAGMENT_SHADER);
	
	/* Compile the shader. */
	
	progv[0] = VProgram;
	glShaderSource(VSHandle, 1, progv, NULL);
	glCompileShader(VSHandle);
	/* Print the compilation log. */
	glGetShaderiv(VSHandle, GL_COMPILE_STATUS, &i);
	if (i != 1) {		
		glGetShaderInfoLog(FSHandle, sizeof(buf), NULL, buf);
		warning("opengl: vertex shader compile failed\n%s\n", buf);
		
		return ENOSYS;
	}
	
	progv[0] = st->prog;
	glShaderSource(FSHandle, 1, progv, NULL);
	glCompileShader(FSHandle);
	
	/* Print the compilation log. */
	glGetShaderiv(FSHandle, GL_COMPILE_STATUS, &i);
	if (i != 1) {
		glGetShaderInfoLog(FSHandle, sizeof(buf), NULL, buf);
		warning("opengl: fragment shader compile failed\n%s\n", buf);
		return ENOSYS;
	}


	/* Create a complete program object. */
	glAttachShader(PHandle, VSHandle);
	glAttachShader(PHandle, FSHandle);
	glLinkProgram(PHandle);

	/* And print the link log. */
	glGetProgramInfoLog(PHandle, sizeof(buf), NULL, buf);

	/* Finally, use the program. */
	glUseProgram(PHandle);

	st->PHandle = PHandle;

	return 0;
}


static int alloc(struct vidisp_st **stp, const struct vidisp *vd,
		 struct vidisp_prm *prm, const char *dev,
		 vidisp_resize_h *resizeh, void *arg)
{
/*
	NSOpenGLPixelFormatAttribute attr[] = {
		NSOpenGLPFAColorSize, 32,
		NSOpenGLPFADepthSize, 16,
		NSOpenGLPFADoubleBuffer,
		0
	};
	NSOpenGLPixelFormat *fmt;
	NSAutoreleasePool *pool;
	* */
	struct vidisp_st *st;
	GLint vsync = 1;
	int err = 0;

	(void)dev;
	(void)resizeh;
	(void)arg;

	/*pool = [[NSAutoreleasePool alloc] init];
	if (!pool)
		return ENOMEM;*/

	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;

	st->vd = vd;

	if (prm && prm->view) {
	}
	else {
		err = create_window(st);
		if (err)
			goto out;
	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	//[pool release];

	return err;
}


static inline void draw_yuv(GLuint PHandle, int height,
			    const uint8_t *Ytex, int linesizeY,
			    const uint8_t *Utex, int linesizeU,
			    const uint8_t *Vtex, int linesizeV)
{
	int i;

	/* This might not be required, but should not hurt. */
	glEnable(GL_TEXTURE_2D);

	/* Select texture unit 1 as the active unit and bind the U texture. */
	glActiveTexture(GL_TEXTURE1);
	i = glGetUniformLocation(PHandle, "Utex");
	glUniform1i(i,1);  /* Bind Utex to texture unit 1 */
	glBindTexture(GL_TEXTURE_2D,1);

	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	//glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
	glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,
		     linesizeU, height/2, 0,
		     GL_LUMINANCE,GL_UNSIGNED_BYTE,Utex);

	/* Select texture unit 2 as the active unit and bind the V texture. */
	glActiveTexture(GL_TEXTURE2);
	i = glGetUniformLocation(PHandle, "Vtex");
	glBindTexture(GL_TEXTURE_2D,2);
	glUniform1i(i,2);  /* Bind Vtext to texture unit 2 */

	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,GL_LINEAR);

	//glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
	glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,
		     linesizeV, height/2, 0,
		     GL_LUMINANCE,GL_UNSIGNED_BYTE,Vtex);

	/* Select texture unit 0 as the active unit and bind the Y texture. */
	glActiveTexture(GL_TEXTURE0);
	i = glGetUniformLocation(PHandle,"Ytex");
	glUniform1i(i,0);  /* Bind Ytex to texture unit 0 */
	glBindTexture(GL_TEXTURE_2D,3);

	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	//glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
		     linesizeY, height, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, Ytex);
}


static inline void draw_blit(int width, int height, int PHandle)
{
	static int VBO = -1;
	if (VBO == -1) {
		//glGenBuffers
	}
	
	glClear(GL_COLOR_BUFFER_BIT);

	GLfloat texcoords[] = {
		0.0, 1.0,
		1.0, 1.0,
		0.0, 0.0,
		1.0, 0.0,
	};

	GLfloat vertices[] = {-1, -1, 0, // bottom left corner
                      1, -1, 0, // top left corner
                       -1, 1, 0, // top right corner
                       1, 1, 0}; // bottom right corner
    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(PHandle, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    
    GLint texAttrib = glGetAttribLocation(PHandle, "texcoord_in");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
   
	//glEnableClientState(GL_VERTEX_ARRAY);
	    
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

#ifndef RASPBERRY_PI

Display* dpy;
Window win;
GLXContext glc;

static int create_gl_window(int width, int height)
{
	Window root;
	XVisualInfo* vi;
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	Colormap cmap;
	XSetWindowAttributes swa;

	dpy = XOpenDisplay(NULL);
 
	 if(dpy == NULL) {
		warning("\n\tcannot connect to X server\n\n");
		return -1;
	 }
        
	root = DefaultRootWindow(dpy);
	vi = glXChooseVisual(dpy, 0, att);

	if(vi == NULL) {
	warning("\n\tno appropriate visual found\n\n");
	return ENOENT;
	}
	else {
        info("\n\tvisual %p selected\n", (void *)vi->visualid);
	}

	cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask;
 
	win = XCreateWindow(dpy, root, 0, 0, width, height, 0, vi->depth,
		InputOutput, vi->visual, CWColormap | CWEventMask, &swa);

	XMapWindow(dpy, win);
	XStoreName(dpy, win, "VERY SIMPLE APPLICATION");
 
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

static void process_gl_window()
{
	XWindowAttributes gwa;
	XEvent xev;
	
	XPeekEvent(dpy, &xev);

	if(xev.type == Expose) {
		XGetWindowAttributes(dpy, win, &gwa);
		//glViewport(0, 0, gwa.width, gwa.height);
		//DrawAQuad(); 
		glXSwapBuffers(dpy, win);
	}
	else if(xev.type == KeyPress) {
		/*glXMakeCurrent(dpy, None, NULL);
		glXDestroyContext(dpy, glc);
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
		exit(0);
		* */
	}
	glXSwapBuffers(dpy, win);
}

#else

EGLDisplay eglDisplay;
EGLSurface surface;

static int create_gl_window(int width, int height)
{
	EGLConfig config;
	EGLContext context;
	
   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   int display_width;
   int display_height;

   // create an EGL window surface, passing context width/height
   int success = graphics_get_display_size(0 /* LCD */, 
                        &display_width, &display_height);
   if ( success < 0 )
   {
	  warning("could not get dispmanx display size\n");
      return EGL_FALSE;
   }

   // You can hardcode the resolution here:
   //display_width = width;
   //display_height = height;
   //display_width = 640;
   //display_height = 480;

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = display_width;
   dst_rect.height = display_height;

   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = display_width << 16;
   src_rect.height = display_height << 16;

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );

   dispman_element = vc_dispmanx_element_add ( dispman_update, 
      dispman_display, 0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 
      0/*clamp*/, 0/*transform*/);

   if (!dispman_element) {
	   warning("Could not add dispmanx element\n");
	   return EGL_FALSE;
   }

   nativewindow.element = dispman_element;
   nativewindow.width = display_width;
   nativewindow.height = display_height;
   vc_dispmanx_update_submit_sync( dispman_update );
   // Pass the window to the display that have been created 
   // to the esContext:
  // esContext->hWnd = &nativewindow;

  eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  int majorVersion, minorVersion;
  eglInitialize(eglDisplay, &majorVersion, &minorVersion);

  info("Initialized EGL %d.%d\n", majorVersion, minorVersion);

  int numConfigs;
  eglGetConfigs(eglDisplay, NULL, 0, &numConfigs);
  eglChooseConfig(eglDisplay, NULL, &config, 1, &numConfigs);
  
     surface = eglCreateWindowSurface(eglDisplay, config, 
                         (EGLNativeWindowType)&nativewindow, NULL);
   if ( surface == EGL_NO_SURFACE )
   {
	   warning("could not create EGL surface\n");
      return EGL_FALSE;
   }
     // 'context' is returned by a "eglCreateContext" call
      EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   context = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );

   // Make the context current
   if ( !eglMakeCurrent(eglDisplay, surface, surface, context) )
   {
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

static void process_gl_window()
{
	 eglSwapBuffers(eglDisplay, surface);
}


#endif 

static int display(struct vidisp_st *st, const char *title,
		   const struct vidframe *frame)
{
	//bool upd = false;
	int err = 0;

	if (!vidsz_cmp(&st->size, &frame->size)) {
		if (st->size.w && st->size.h) {
			info("opengl: reset: %u x %u  --->  %u x %u\n",
			     st->size.w, st->size.h,
			     frame->size.w, frame->size.h);
		}
		//SDL_SetVideoMode(frame->size.w,frame->size.h,32,SDL_HWSURFACE|SDL_ANYFORMAT|SDL_OPENGL);
		/*st->window = SDL_CreateWindow("test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            frame->size.w, frame->size.h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetSwapInterval(0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		SDL_GL_CreateContext(st->window);

		SDL_CreateRenderer(
		st->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
		* */

		err = create_gl_window(frame->size.w, frame->size.h);

		if (!err) {
			warning("EGL Context Creation failed\n");
		}

		opengl_reset(st, &frame->size);

		//upd = true;
	}

	if (frame->fmt == VID_FMT_YUV420P) {

		if (!st->PHandle) {

			debug("opengl: using Vertex shader with YUV420P\n");

			err = setup_shader(st, frame->size.w, frame->size.h);
			if (err)
				goto out;
		}

		draw_yuv(st->PHandle, frame->size.h,
			 frame->data[0], frame->linesize[0],
			 frame->data[1], frame->linesize[1],
			 frame->data[2], frame->linesize[2]);
		draw_blit(frame->size.w, frame->size.h, st->PHandle);
	}
	else {
		warning("opengl: unknown pixel format %s\n",
			vidfmt_name(frame->fmt));
		err = EINVAL;
	}

	//SDL_GL_SwapWindow(st->window);
	process_gl_window();
out:
	return err;
}


static void hide(struct vidisp_st *st)
{
	if (!st)
		return;
}


static int module_init(void)
{
	int err;

	//SDL_Init(SDL_INIT_VIDEO);
	//SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
#ifdef RASPBERRY_PI
	bcm_host_init();
#endif

	err = vidisp_register(&vid, baresip_vidispl(),
			      "gltexture", alloc, NULL, display, hide);
	if (err)
		return err;

	return 0;
}


static int module_close(void)
{
	vid = mem_deref(vid);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(gltexture) = {
	"gltexture",
	"vidisp",
	module_init,
	module_close,
};
