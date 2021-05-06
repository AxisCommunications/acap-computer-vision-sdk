# Compiles all proto files from TensorFlow
# Outputs .o, .h, .cc and .proto files.

RM ?= rm -f
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))


GRPC_CPP_PLUGIN_PATH ?= /usr/bin/grpc_cpp_plugin

PROTOBUF_FILES := $(call rwildcard, $(API_PATH),*.proto)
PROTOBUF_FILES_OUT := $(patsubst $(API_PATH)%,$(OUT_PATH)%,$(PROTOBUF_FILES))
PROTOBUF_CC := $(patsubst %.proto,%.pb.cc,$(PROTOBUF_FILES_OUT))
PROTOBUF_O := $(patsubst %.pb.cc,%.pb.o,$(PROTOBUF_CC))
PROTOBUF_GRPC_O := $(patsubst %.pb.cc,%.grpc.pb.o,$(PROTOBUF_CC))

# $(info $(PROTOBUF_GRPC_O))

# Compiler flags
CXXFLAGS += -std=c++17 -I$(OUT_PATH) -I/usr/include -Wno-psabi -Os

.PHONY: clean default

# Keep all intermediates (.cc and .proto)
.SECONDARY:

# Copy .proto files
$(OUT_PATH)/%.proto: $(API_PATH)/%.proto 
	mkdir -p $(@D)
	cp $< $@

$(OUT_PATH)/%.pb.cc $(OUT_PATH)/%.pb.h: $(API_PATH)/%.proto $(OUT_PATH)/%.proto | $(PROTOBUF_FILES_OUT)
	protoc -I$(API_PATH) --cpp_out=$(OUT_PATH) $<

$(OUT_PATH)/%.grpc.pb.cc $(OUT_PATH)/%.grpc.pb.h: $(API_PATH)/%.proto $(OUT_PATH)/%.proto | $(PROTOBUF_FILES_OUT)
	protoc -I$(API_PATH) --grpc_out=$(OUT_PATH) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

# Build protobuf object files
%.pb.o: %.pb.cc $(PROTOBUF_CC)
	$(CXX) -c $(CXXFLAGS) -I$(OUT_PATH)  $< -o $@

default: clean $(PROTOBUF_O) $(PROTOBUF_GRPC_O)

clean:
	$(RM) -r $(OUT_PATH)

