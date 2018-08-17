################################################################################
#
# Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.
#
################################################################################

# Target rules
all: build

build: server

server: server.cpp
	$(GCC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

server: AppDecGL.o Utils.o
	$(GCC) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf server ColorSpace.o FoV.o Utils.o AppDecGL.o Resize.o ReShape.o NvDecoder.o NvEncoder.o NvEncoderCuda.o
