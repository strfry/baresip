/**
 * @file opengl.c Video driver for OpenGL on MacOSX
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */

#define GL_GLEXT_PROTOTYPES
//#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
//#include <SDL2/SDL_opengl.h>
//#include <GL/gl.h>
//#include <GL/glext.h>
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
	SDL_Window* window;
};


static struct vidisp *vid;       /**< OPENGL Video-display      */

static const char* VProgram =
	"attribute vec2 position;    \n"
	"void main()                  \n"
	"{                            \n"
	"   gl_Position = vec4(position, 0.0, 1.0);  \n"
	"}                            \n";

    //
  //"  //nx=gl_TexCoord[0].x;\n"
  //"  //ny=(%d.0-gl_TexCoord[0].y);\n"
    //
static const char *FProgram=
  "uniform sampler2D Ytex;\n"
  "uniform sampler2D Utex,Vtex;\n"
  "void main(void) {\n"
  "  float nx,ny,r,g,b,y,u,v;\n"
  "  vec4 txl,ux,vx;"
  "	 float width = %d.0;      \n"
  "	 float height = %d.0;      \n"
  "  nx=gl_FragCoord.x / width;\n"
  "  ny=(height - gl_FragCoord.y) / height;\n"
  "  y=texture2D(Ytex,vec2(nx,ny)).r;\n"
  "  u=texture2D(Utex,vec2(nx/2.0,ny/2.0)).r;\n"
  "  v=texture2D(Vtex,vec2(nx/2.0,ny/2.0)).r;\n"

  "  y=1.1643*(y-0.0625);\n"
  "  u=u-0.5;\n"
  "  v=v-0.5;\n"

  "  r=y+1.5958*v;\n"
  "  g=y-0.39173*u-0.81290*v;\n"
  "  b=y+2.017*u;\n"

  "  gl_FragColor=vec4(r,g,b,1.0);\n"
  "}\n";


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
		warning("opengl: vertex shader compile failed\n");
		return ENOSYS;
	}
	
	progv[0] = st->prog;
	glShaderSource(FSHandle, 1, progv, NULL);
	glCompileShader(FSHandle);
	
	/* Print the compilation log. */
	glGetShaderiv(FSHandle, GL_COMPILE_STATUS, &i);
	if (i != 1) {
		warning("opengl: fragment shader compile failed\n");
		return ENOSYS;
	}

	glGetShaderInfoLog(FSHandle, sizeof(buf), NULL, buf);

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

	/*fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
	if (!fmt) {
		err = ENOMEM;
		warning("opengl: Failed creating OpenGL format\n");
		goto out;
	} */

	//st->ctx = [[NSOpenGLContext alloc] initWithFormat:fmt
	//				   shareContext:nil];

	//[fmt release];

/*
	if (!st->ctx) {
		err = ENOMEM;
		warning("opengl: Failed creating OpenGL context\n");
		goto out;
	}
*/

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
		0, 0,
		1.0, 0,
		0, 1.0,
		1.0, 1.0,
	};

	GLfloat vertices[] = {-1, -1, 0, // bottom left corner
                      1, -1, 0, // top left corner
                       -1, 1, 0, // top right corner
                       1, 1, 0}; // bottom right corner
    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(PHandle, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, vertices);
   
	//glEnableClientState(GL_VERTEX_ARRAY);
	    
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

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
		st->window = SDL_CreateWindow("test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            frame->size.w, frame->size.h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetSwapInterval(0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		SDL_GL_CreateContext(st->window);

		SDL_CreateRenderer(
		st->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

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

	SDL_GL_SwapWindow(st->window);
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

	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

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
