NAME     := rokuyon
BUILD    := build
SOURCES  := src src/desktop
ARGS     := -O3 -flto -std=c++11 -DLOG_LEVEL=0
LIBS     := $(shell pkg-config --libs portaudio-2.0)
INCLUDES := $(shell pkg-config --cflags portaudio-2.0)

ifeq ($(OS),Windows_NT)
  ARGS += -static
  LIBS += $(shell wx-config-static --libs core,gl) -lole32 -lsetupapi -lwinmm
  INCLUDES += $(shell wx-config-static --cxxflags core,gl)
else
  LIBS += $(shell wx-config --libs core,gl)
  INCLUDES += $(shell wx-config --cxxflags core,gl)
  ifneq ($(shell uname -s),Darwin)
    LIBS += -lGL
  endif
endif

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))
OFILES   := $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

all: $(NAME)

$(NAME): $(OFILES)
	g++ -o $@ $(ARGS) $^ $(LIBS)

$(BUILD)/%.o: %.cpp $(HFILES) $(BUILD)
	g++ -c -o $@ $(ARGS) $(INCLUDES) $<

$(BUILD):
	for dir in $(SOURCES); \
	do \
	mkdir -p $(BUILD)/$$dir; \
	done

clean:
	rm -rf $(BUILD)
	rm -f $(NAME)
