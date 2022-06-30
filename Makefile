NAME     := rokuyon
BUILD    := build
SOURCES  := src src/desktop
ARGS     := -Ofast -flto -std=c++11
LIBS     := $(shell wx-config --libs --gl-libs) -lGL
INCLUDES := $(shell wx-config --cxxflags)

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
