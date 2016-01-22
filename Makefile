# OPT ?= -O2 -DNDEBUG     # (A) Production use (optimized mode)
OPT ?= -g2 -Wall -Werror  # (B) Debug mode, w/ full line-level debugging symbols
# OPT ?= -O2 -g2 -DNDEBUG # (C) Profiling mode: opt, but w/debugging symbols

# Thirdparty
include depends.mk

CC = gcc
CXX = g++

SHARED_CFLAGS = -fPIC
SHARED_LDFLAGS = -shared -Wl,-soname -Wl,

INCPATH += -I./kernel/src $(DEPS_INCPATH) 
CFLAGS += -std=c99 $(OPT) $(SHARED_CFLAGS) $(INCPATH)
CXXFLAGS += $(OPT) $(SHARED_CFLAGS) $(INCPATH)
LDFLAGS += -rdynamic $(DEPS_LDPATH) $(DEPS_LDFLAGS) -lpthread -lrt -lz 


PREFIX=./output


KERNEL_PROTO_FILE = $(wildcard kernel/src/proto/*.proto)
KERNEL_PROTO_SRC = $(patsubst %.proto,%.pb.cc,$(KERNEL_PROTO_FILE))
KERNEL_PROTO_HEADER = $(patsubst %.proto,%.pb.h,$(KERNEL_PROTO_FILE))
KERNEL_PROTO_OBJ = $(patsubst %.proto,%.pb.o,$(KERNEL_PROTO_FILE))

KERNEL_MASTER_SRC = $(wildcard kernel/src/master/*.cc)
KERNEL_MASTER_OBJ = $(patsubst %.cc, %.o, $(KERNEL_MASTER_SRC))
KERNEL_MASTER_HEADER = $(wildcard kernel/src/master/*.h)

KERNEL_AGENT_SRC = $(wildcard kernel/src/agent/*.cc)
KERNEL_AGENT_OBJ = $(patsubst %.cc, %.o, $(KERNEL_AGENT_SRC))
KERNEL_AGENT_HEADER = $(wildcard kernel/src/agent/*.h)


KERNEL_ENGINE_SRC = $(wildcard kernel/src/engine/*.cc)
KERNEL_ENGINE_OBJ = $(patsubst %.cc, %.o, $(KERNEL_ENGINE_SRC))
KERNEL_ENGINE_HEADER = $(wildcard kernel/src/engine/*.h)



KERNEL_FLAGS_OBJ = $(patsubst %.cc, %.o, $(wildcard kernel/src/*.cc))
KERNEL_OBJS = $(KERNEL_FLAGS_OBJ) $(KERNEL_PROTO_OBJ)

BIN = dos_master dos_let dos_ce 

all: $(BIN) 

# Depends
$(KERNEL_MASTER_OBJ) $(KERNEL_AGENT_OBJ): $(KERNEL_PROTO_HEADER)

# Targets
dos_master: $(KERNEL_MASTER_OBJ) $(KERNEL_OBJS)
	$(CXX) $(KERNEL_MASTER_OBJ)  $(KERNEL_OBJS) -o $@  $(LDFLAGS)

dos_let: $(KERNEL_AGENT_OBJ)  $(KERNEL_OBJS)
	$(CXX) $(KERNEL_AGENT_OBJ) $(KERNEL_OBJS) -o $@ $(LDFLAGS)


dos_ce: $(KERNEL_ENGINE_OBJ)  $(KERNEL_OBJS)
	$(CXX) $(KERNEL_ENGINE_OBJ) $(KERNEL_OBJS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.pb.h %.pb.cc: %.proto
	$(PROTOC) --proto_path=./kernel/src/proto/  --cpp_out=./kernel/src/proto/ $<

clean:
	rm -rf $(BIN)
	rm -rf $(KERNEL_MASTER_OBJ) $(KERNEL_AGENT_OBJ) $(KERNEL_OBJS) $(KERNEL_ENGINE_OBJ)
	rm -rf $(KERNEL_PROTO_SRC) $(KERNEL_PROTO_HEADER)

install: $(BIN) $(LIBS)
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include/sdk
	cp $(BIN) $(PREFIX)/bin
	cp $(LIBS) $(PREFIX)/lib
	cp src/sdk/*.h $(PREFIX)/include/sdk

.PHONY: test
test:
	echo done


