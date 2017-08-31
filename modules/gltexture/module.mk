#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= gltexture
$(MOD)_SRCS	+= opengl.c
$(MOD)_LFLAGS	+= -lGL

include mk/mod.mk
