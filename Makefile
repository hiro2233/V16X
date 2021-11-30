WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = 

INC = -Ilibraries -Ilibraries/UR_V16X
CFLAGS = -Wshadow -Wall -std=gnu++11 -fpermissive -D__STDC_FORMAT_MACROS -DENABLE_SYSTEM_SHUTDOWN -DCRYPTON_TYPE=CRYPTON_NATIVE
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = -pthread

# RELEASE
INC_UNIX_RELEASE = $(INC)
CFLAGS_UNIX_RELEASE = $(CFLAGS) -O2 -fdata-sections -ffunction-sections -fno-exceptions -fsigned-char -D_UNIX_
RESINC_UNIX_RELEASE = $(RESINC)
RCFLAGS_UNIX_RELEASE = $(RCFLAGS)
LIBDIR_UNIX_RELEASE = $(LIBDIR)
LIB_UNIX_RELEASE = $(LIB) 
LDFLAGS_UNIX_RELEASE = $(LDFLAGS) -s -Wl,--gc-sections
OBJDIR_UNIX_RELEASE = build/.obj/release
DEP_UNIX_RELEASE = 
OUT_UNIX_RELEASE = build/release/v16x

OBJ_UNIX_RELEASE = $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Driver.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/system.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing/netsocket_win.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Posix.o $(OBJDIR_UNIX_RELEASE)/app/main.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_DeepService.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Openssl.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Native.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Backend.o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton.o $(OBJDIR_UNIX_RELEASE)/app/v16x_server.o


# DEBUG
INC_UNIX_DEBUG = $(INC)
CFLAGS_UNIX_DEBUG = $(CFLAGS) -g -D_UNIX_
RESINC_UNIX_DEBUG = $(RESINC)
RCFLAGS_UNIX_DEBUG = $(RCFLAGS)
LIBDIR_UNIX_DEBUG = $(LIBDIR)
LIB_UNIX_DEBUG = $(LIB) 
LDFLAGS_UNIX_DEBUG = $(LDFLAGS)
OBJDIR_UNIX_DEBUG = build/.obj/debug
DEP_UNIX_DEBUG = 
OUT_UNIX_DEBUG = build/debug/v16x

OBJ_UNIX_DEBUG = $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Driver.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/system.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing/netsocket_win.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Posix.o $(OBJDIR_UNIX_DEBUG)/app/main.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_DeepService.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Openssl.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Native.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Backend.o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton.o $(OBJDIR_UNIX_DEBUG)/app/v16x_server.o


all: unix_release unix_debug

clean: clean_unix_release clean_unix_debug

before_unix_release: 
	test -d build/release || mkdir -p build/release
	test -d $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X || mkdir -p $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X
	test -d $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system || mkdir -p $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system
	test -d $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing || mkdir -p $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing
	test -d $(OBJDIR_UNIX_RELEASE)/app || mkdir -p $(OBJDIR_UNIX_RELEASE)/app
	test -d $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton || mkdir -p $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton

after_unix_release: 

unix_release: before_unix_release out_unix_release after_unix_release

out_unix_release: before_unix_release $(OBJ_UNIX_RELEASE) $(DEP_UNIX_RELEASE)
	$(LD) $(LIBDIR_UNIX_RELEASE) -o $(OUT_UNIX_RELEASE) $(OBJ_UNIX_RELEASE)  $(LDFLAGS_UNIX_RELEASE) $(LIB_UNIX_RELEASE)

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Driver.o: libraries/UR_V16X/UR_V16X_Driver.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/UR_V16X_Driver.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Driver.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/system.o: libraries/UR_V16X/system/system.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/system/system.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/system.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing/netsocket_win.o: libraries/UR_V16X/system/missing/netsocket_win.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/system/missing/netsocket_win.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing/netsocket_win.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Posix.o: libraries/UR_V16X/UR_V16X_Posix.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/UR_V16X_Posix.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_Posix.o

$(OBJDIR_UNIX_RELEASE)/app/main.o: app/main.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c app/main.cpp -o $(OBJDIR_UNIX_RELEASE)/app/main.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_DeepService.o: libraries/UR_V16X/UR_V16X_DeepService.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/UR_V16X_DeepService.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X_DeepService.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X.o: libraries/UR_V16X/UR_V16X.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_V16X/UR_V16X.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/UR_V16X.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Openssl.o: libraries/UR_Crypton/UR_Crypton_Openssl.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_Crypton/UR_Crypton_Openssl.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Openssl.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Native.o: libraries/UR_Crypton/UR_Crypton_Native.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_Crypton/UR_Crypton_Native.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Native.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Backend.o: libraries/UR_Crypton/UR_Crypton_Backend.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_Crypton/UR_Crypton_Backend.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton_Backend.o

$(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton.o: libraries/UR_Crypton/UR_Crypton.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c libraries/UR_Crypton/UR_Crypton.cpp -o $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton/UR_Crypton.o

$(OBJDIR_UNIX_RELEASE)/app/v16x_server.o: app/v16x_server.cpp
	$(CXX) $(CFLAGS_UNIX_RELEASE) $(INC_UNIX_RELEASE) -c app/v16x_server.cpp -o $(OBJDIR_UNIX_RELEASE)/app/v16x_server.o

clean_unix_release: 
	rm -f $(OBJ_UNIX_RELEASE) $(OUT_UNIX_RELEASE)
	rm -rf build/release
	rm -rf $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X
	rm -rf $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system
	rm -rf $(OBJDIR_UNIX_RELEASE)/libraries/UR_V16X/system/missing
	rm -rf $(OBJDIR_UNIX_RELEASE)/app
	rm -rf $(OBJDIR_UNIX_RELEASE)/libraries/UR_Crypton

before_unix_debug: 
	test -d build/debug || mkdir -p build/debug
	test -d $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X || mkdir -p $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X
	test -d $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system || mkdir -p $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system
	test -d $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing || mkdir -p $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing
	test -d $(OBJDIR_UNIX_DEBUG)/app || mkdir -p $(OBJDIR_UNIX_DEBUG)/app
	test -d $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton || mkdir -p $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton

after_unix_debug: 

unix_debug: before_unix_debug out_unix_debug after_unix_debug

out_unix_debug: before_unix_debug $(OBJ_UNIX_DEBUG) $(DEP_UNIX_DEBUG)
	$(LD) $(LIBDIR_UNIX_DEBUG) -o $(OUT_UNIX_DEBUG) $(OBJ_UNIX_DEBUG)  $(LDFLAGS_UNIX_DEBUG) $(LIB_UNIX_DEBUG)

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Driver.o: libraries/UR_V16X/UR_V16X_Driver.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/UR_V16X_Driver.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Driver.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/system.o: libraries/UR_V16X/system/system.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/system/system.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/system.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing/netsocket_win.o: libraries/UR_V16X/system/missing/netsocket_win.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/system/missing/netsocket_win.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing/netsocket_win.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Posix.o: libraries/UR_V16X/UR_V16X_Posix.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/UR_V16X_Posix.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_Posix.o

$(OBJDIR_UNIX_DEBUG)/app/main.o: app/main.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c app/main.cpp -o $(OBJDIR_UNIX_DEBUG)/app/main.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_DeepService.o: libraries/UR_V16X/UR_V16X_DeepService.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/UR_V16X_DeepService.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X_DeepService.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X.o: libraries/UR_V16X/UR_V16X.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_V16X/UR_V16X.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/UR_V16X.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Openssl.o: libraries/UR_Crypton/UR_Crypton_Openssl.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_Crypton/UR_Crypton_Openssl.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Openssl.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Native.o: libraries/UR_Crypton/UR_Crypton_Native.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_Crypton/UR_Crypton_Native.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Native.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Backend.o: libraries/UR_Crypton/UR_Crypton_Backend.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_Crypton/UR_Crypton_Backend.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton_Backend.o

$(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton.o: libraries/UR_Crypton/UR_Crypton.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c libraries/UR_Crypton/UR_Crypton.cpp -o $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton/UR_Crypton.o

$(OBJDIR_UNIX_DEBUG)/app/v16x_server.o: app/v16x_server.cpp
	$(CXX) $(CFLAGS_UNIX_DEBUG) $(INC_UNIX_DEBUG) -c app/v16x_server.cpp -o $(OBJDIR_UNIX_DEBUG)/app/v16x_server.o

clean_unix_debug: 
	rm -f $(OBJ_UNIX_DEBUG) $(OUT_UNIX_DEBUG)
	rm -rf build/debug
	rm -rf $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X
	rm -rf $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system
	rm -rf $(OBJDIR_UNIX_DEBUG)/libraries/UR_V16X/system/missing
	rm -rf $(OBJDIR_UNIX_DEBUG)/app
	rm -rf $(OBJDIR_UNIX_DEBUG)/libraries/UR_Crypton

.PHONY: before_unix_release after_unix_release clean_unix_release before_unix_debug after_unix_debug clean_unix_debug
