CC = emcc
CXX = em++
EXE = websocksse.html
SOURCES =  websocksse.cpp
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))
UNAME_S := $(shell uname -s)

EMS += -s WASM=0 -s EXTRA_EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','getValue'] -s ASYNCIFY=0
EMS += -s EXPORT_ALL=1 -s EXPORTED_FUNCTIONS=['_main']
EMS += -s LEGACY_GL_EMULATION=0 -s GL_UNSAFE_OPTS=0 -s NO_FILESYSTEM=1 -s FORCE_FILESYSTEM=0
EMS += -s ALLOW_MEMORY_GROWTH=0 -s WEBSOCKET_SUBPROTOCOL=null -s MODULARIZE=0 -s SINGLE_FILE=0
EMS += -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s USE_SDL=0
#EMS += --preload-file data.txt
#EMS += -s ASSERTIONS=0

#CPPFLAGS += -g
CPPFLAGS = -Wall -Wformat -O2 -ffunction-sections -fdata-sections
CPPFLAGS += $(EMS)
#CPPFLAGS += -fno-exceptions

LIBS += $(EMS)
#LIBS += -lidbfs.js
LDFLAGS += --shell-file websocksse_shell.html -Wl,--gc-sections
#LDFLAGS += -fno-exceptions

$(shell mkdir -p ../../build/websocksse)

%.o:%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o ../../build/websocksse/$@ $<

all: $(EXE)
	@echo Build complete for $(EXE)

$(EXE): $(OBJS)
	$(shell mkdir -p ../../htdocs/websocksse && cp globals.js ../../htdocs/websocksse/ && cp favicon.ico ../../htdocs/)
	$(CXX) -o ../../htdocs/websocksse/$@ ../../build/websocksse/*.o $(LIBS) $(LDFLAGS)

clean:
	rm -f $(EXE) ../../build/websocksse/*.o $(OBJS) *.wasm *.wasm.pre
