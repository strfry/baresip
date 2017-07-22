#
# module.mk
#
# Copyright (C) 2017 Creytiv.com
#

MOD		:= webrtc_aec
$(MOD)_SRCS	+= webrtc_aec.cpp
$(MOD)_LFLAGS	+= -lwebrtc_audio_processing -fPIC
$(MOD)_CXXFLAGS	+= -fPIC -fno-exceptions -fno-rtti -g

include mk/mod.mk
