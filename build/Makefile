TARGET	= main
CXXSRCS	= $(wildcard ../src/*.cpp)
CSRCS	= $(wildcard ../vl/*.c)
CHEADERS	= $(wildcard ../src/*.h)
CXXHEADERS = $(wildcard ../vl/*.h)
CXXDIR	= $(notdir $(CXXSRCS))
CDIR = $(notdir $(CSRCS))
CXXOBJECTS	= $(patsubst %.cpp,%.o, $(CXXDIR)) 
COBJECTS	= $(patsubst %.c,%.o, $(CDIR)) 

CUDA_ROOT = /usr/local/cuda
PCA_ROOT = ../libpca-1.2.11
SIFT_ROOT = ../CudaSift

INC =-Ivl/
INC +=-I$(PCA_ROOT)/include -I$(SIFT_ROOT)/include

LIB     = -lm -lrt -lpthread -ldl -pipe -L$(SIFT_ROOT) -Wl,-RCudaSift '-Wl,-R$$ORIGIN' -lcudasift -std=c++0x -pthread
LIB     += -L$(CUDA_ROOT)/lib64/ -lcudart -lcufft -lpca
LIB += `pkg-config --cflags --libs opencv`
LIB += -fopenmp -llapack -lcublas

NVCC	= $(CUDA_ROOT)/bin/nvcc -arch=sm_61 -m 64 $(INC)

CXX = g++ -pipe
CC = gcc -pipe
LINK	= -O3 -pthread 
CFLAGS	= -O3 -c -pthread -fopenmp -DVL_DISABLE_AVX
CXXFLAGS= -O3 -c -pthread -std=c++0x -fopenmp -DVL_DISABLE_AVX

gpu_fv: $(COBJECTS) $(CXXOBJECTS) cuda_files.o
	$(CXX) $(LINK) -o ../$@ $(COBJECTS) $(CXXOBJECTS) cuda_files.o $(LIB) 

cuda_files.o: ../src/cuda_files.cu 
	$(NVCC) -I$(CUDA_ROOT)/include/  -O3 -g -G -c ../src/cuda_files.cu -lcublas 

$(COBJECTS): $(CSRCS) $(CHEADERS) Makefile
	$(CC) $(INC) $(CFLAGS) $(CSRCS) 

$(CXXOBJECTS): $(CXXSRCS) $(CXXHEADERS) Makefile
	$(CXX) $(INC) $(CXXFLAGS) $(CXXSRCS)

clean:
	rm -f ../gpu_fv $(COBJECTS) $(CXXOBJECTS) cuda_files.o kernels.o

.DEFAULT:
	@echo The target \"$@\" does not exist in Makefile.

