#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= gltexture
$(MOD)_SRCS	+= opengl.c
$(MOD)_CFLAGS   += -I /opt/vc/include
$(MOD)_LFLAGS	+= -lGLESv2

include mk/mod.mk
