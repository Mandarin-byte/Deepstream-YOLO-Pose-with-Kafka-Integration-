################################################################################
# Makefile for custom nvmsgconv library for Pose Estimation
################################################################################

CXX:= g++

SRCS:= nvmsg_conv_pose.cpp

TARGET_LIB:= libnvds_msgconv_pose.so

CFLAGS:= -fPIC -std=c++14

CFLAGS+= -I/opt/nvidia/deepstream/deepstream/sources/includes \
         -I/opt/nvidia/deepstream/deepstream-7.0/sources/libs/nvmsgconv \
         -I/opt/nvidia/deepstream/deepstream-7.0/sources/libs/nvmsgconv/deepstream_schema \
         -I/usr/local/cuda/include

CFLAGS+= `pkg-config --cflags glib-2.0 json-glib-1.0 uuid`

LIBS:= `pkg-config --libs glib-2.0 json-glib-1.0 uuid`

LIBS+= -L/opt/nvidia/deepstream/deepstream/lib \
       -lnvds_meta -lnvds_utils

LDFLAGS:= -shared

OBJS:= $(SRCS:.cpp=.o)

all: $(TARGET_LIB)

%.o: %.cpp
	$(CXX) -c -o $@ $(CFLAGS) $<

$(TARGET_LIB): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -rf $(OBJS) $(TARGET_LIB)

.PHONY: all clean
