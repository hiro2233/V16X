CC = emcc
CXX = em++
EXE = hello.html
IMGUI_DIR = ../modules/imgui
SOURCES =  hello.cpp camera.cpp events.cpp
SOURCES_IMGUI = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES_IMGUI += $(IMGUI_DIR)/backends/imgui_impl_sdl.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))) $(basename $(notdir $(SOURCES_IMGUI))))
UNAME_S := $(shell uname -s)

EMS += -s USE_SDL=2 -s WASM=1 -s EXTRA_EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -s ASYNCIFY=1 --preload-file data.txt
EMS += -s EXPORT_ALL=1 -s EXPORTED_FUNCTIONS=['_Sum_ccall','_main','_set_cube_angle','_Sum','_draw_cube','_stop_drawing','_get_drawing']
EMS += -s FORCE_FILESYSTEM=1 -s LEGACY_GL_EMULATION=1 -DIMGUI_DISABLE_FILE_FUNCTIONS -s GL_UNSAFE_OPTS=0
EMS += -s ALLOW_MEMORY_GROWTH=1 -s WEBSOCKET_SUBPROTOCOL=null

CPPFLAGS = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
#CPPFLAGS += -g
CPPFLAGS += -Wall -Wformat -O3
CPPFLAGS += $(EMS)
LIBS += $(EMS) -lwebsocket.js
#LIBS += -lidbfs.js
LDFLAGS += --shell-file hello_shell.html

%.o:%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o ../build/$@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o ../build/$@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o ../build/$@ $<

%.o:$(IMGUI_DIR)/libs/gl3w/GL/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o ../build/$@ $<

all: $(EXE)
	@echo Build complete for $(EXE)

$(EXE): $(OBJS)
	$(shell mkdir -p ../htdocs/static && cp globals.js ../htdocs/static/ && cp favicon.ico ../htdocs/)
	$(CXX) -o ../htdocs/static/$@ ../build/*.o $(LIBS) $(LDFLAGS)

clean:
	rm -f $(EXE) ../build/*.o $(OBJS) *.wasm *.wasm.pre
