
CXX = g++
CXXFLAGS += -lpthread -I ../../thirdparty/include 
CXXFLAGS += -std=c++11
LDFLAGS += -L ../../thirdparty/lib  -lgrpc++_unsecure -lgrpc -lgpr -lprotobuf -lpthread -ldl

PROTOC = ../../thirdparty/bin/protoc
GRPC_CPP_PLUGIN = ../../thirdparty/bin/grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
PROTO_PATH = ./src/proto





%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTO_PATH) --grpc_out=$(PROTO_PATH) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(PROTO_PATH) --cpp_out=$(PROTO_PATH) $<
