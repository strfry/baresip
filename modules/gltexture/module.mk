#
# module.mk
#
# Copyright (C) 2010 Creytiv.com
#

MOD		:= gltexture
$(MOD)_SRCS	+= opengl.c

ifneq ($(USE_OMX_RPI),)
$(MOD)_CFLAGS   += -I /opt/vc/include -DRASPBERRY_PI
#$(MOD)_CFLAGS   += -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT
#$(MOD)_CFLAGS   += -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -I./ -I/opt/vc/src/hello_pi/libs/ilclient -I/opt/vc/src/hello_pi/libs/vgfont
$(MOD)_LFLAGS	+= -lbrcmGLESv2 -lbrcmEGL
endif

include mk/mod.mk
