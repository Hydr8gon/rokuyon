NAME     := rokuyon
BUILD    := build
SOURCES  := src src/desktop
ARGS     := -O3 -flto -std=c++11 -DLOG_LEVEL=0
LIBS     := $(shell wx-config --libs --gl-libs) $(shell pkg-config --libs portaudio-2.0) -lGL
INCLUDES := $(shell wx-config --cxxflags) $(shell pkg-config --cflags portaudio-2.0)

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
