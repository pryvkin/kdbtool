MODULES   := main lib crypto plugins
SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_DIR := $(addprefix build/,$(MODULES))

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
CSRC      := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ       := $(patsubst src/%.cpp,build/%.o,$(SRC))
COBJ      := $(patsubst src/%.c,build/%.o,$(CSRC))
INCLUDES  := -Isrc/ $(addprefix -I,$(SRC_DIR))

PROG = kdbtool

CC = gcc
CXX = g++

QT_CXXFLAGS = `pkg-config --cflags QtCore`
QT_CXXFLAGS += `pkg-config --cflags QtGui` 
#QT_CXXFLAGS += `pkg-config --cflags QtXml` 

QT_LIB = `pkg-config --libs QtCore`
QT_LIB += `pkg-config --libs QtGui` 
#QT_LIB += `pkg-config --libs QtXml` 

CXXFLAGS = -std=c++0x -g $(QT_CXXFLAGS)  $(INCLUDES)
CFLAGS = -g $(QT_CXXFLAGS)  $(INCLUDES)

LIB = $(QT_LIB)

vpath %.cpp $(SRC_DIR)
vpath %.c $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $$< -o $$@
endef
define cmake-goal
$1/%.o: %.c
	$(CC) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

all: Makefile checkdirs $(PROG)

$(PROG): $(OBJ) $(COBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIB)

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
$(foreach bdir,$(BUILD_DIR),$(eval $(call cmake-goal,$(bdir))))
