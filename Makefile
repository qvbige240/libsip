.SUFFIXES : .x .o .c .s

include env.conf

ROOT=$(shell /bin/pwd)

#CC=gcc
#STRIP=strip

ifeq ($(DIR_PJSIP), )
DIR_PJSIP=$(ROOT)/..
endif

all: test sip

ifeq ($(PLATFORM), x86)
INSTALL_PJ=$(DIR_PJSIP)/final_x86
else ifeq ($(PLATFORM), nt966x)
INSTALL_PJ=$(DIR_PJSIP)/final_nt966x
else ifeq ($(PLATFORM), nt966x_d048)
INSTALL_PJ=$(DIR_PJSIP)/final_nt966x
else
PLATFORM=x86
INSTALL_PJ=$(DIR_PJSIP)/final_x86
endif

test:
	@echo "======$(DIR_PJSIP)"
	@echo "======WORKDIR: $(WORKDIR) "
	@echo "======WORKDIR: ${WORKDIR} "
	@echo "======DIR_PREMAKE: $(DIR_PREMAKE)"
	@echo "======DIR_PREMAKE: ${DIR_PREMAKE}"
	@echo "========BUILD_PATH: $(BUILD_PATH)"
	@echo "========BUILD_PATH: ${BUILD_PATH}"
	@echo "========FINAL_PATH: $(FINAL_PATH)"
	@echo "========GBASE_LIB: $(GBASE_LIB)"
	@echo "========GBASE_INCLUDE: $(GBASE_INCLUDE)"
	@echo "========GOLBAL_CFLAGS: $(GOLBAL_CFLAGS)"
	@echo "========GOLBAL_CPPFLAGS: $(GOLBAL_CPPFLAGS)"
	@echo "========GOLBAL_LDFLAGS: $(GOLBAL_LDFLAGS)"
	@echo "========PLATFORM: $(PLATFORM)"
	@echo "========CC: $(CC)"

#INSTALL_PJ=$(DIR_PJSIP)/final_$(PLATFORM)

LIB_PJ = $(INSTALL_PJ)/lib
INC_PJ = $(INSTALL_PJ)/include
INC = $(ROOT)
INC_SIP = $(ROOT)/inc
#INC_SYS = /usr/include
#LIB_SYS = /usr/lib

#SIP_CFLAGS=-g -O0 -DUSE_ZLOG -I$(INC_PJ) -I$(INC1) -I$(INC_SIP) -I$(INC_SYS)
#WEC_LDFLAGS=-L$(GBASE_LIB) -L$(LIB_PJ)  
SIP_CFLAGS  = -g -O0 -DUSE_ZLOG -I$(INC_PJ) -I$(INC) -I$(INC_SIP) -DSIP_CONFIG_ANDROID=0 $(GOLBAL_CFLAGS) -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1
#SIP_CFLAGS += -DSIP_SERVER_INTERNAL
#SIP_CFLAGS += -DSIP_SERVER_DEVELOP_ENV
SIP_CFLAGS += -DSIP_SERVER_TEST_ENV
WEC_LDFLAGS = $(GOLBAL_LDFLAGS) -L$(LIB_PJ)

TARGET = libsip.so
SRCS := sip_client.c sip_register.c

DEPEND_LIB_X86 = $(LIB_PJ)/libpjsip-ua-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsdp-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-simple-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-x86_64-unknown-linux-gnu.a \
 $(LIB_PJ)/libpjlib-util-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpj-x86_64-unknown-linux-gnu.a

DEPEND_LIB_NT966X = $(LIB_PJ)/libpjsip-ua-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjsip-simple-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjsdp-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjsip-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjlib-util-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpj-mipsel-24kec-linux-uclibc.a

ifeq ($(PLATFORM), x86)
DEPEND_LIB = $(DEPEND_LIB_X86) -luuid
else ifeq ($(PLATFORM), nt966x)
DEPEND_LIB = $(DEPEND_LIB_NT966X)
else ifeq ($(PLATFORM), nt966x_d048)
DEPEND_LIB = $(DEPEND_LIB_NT966X)
endif

LIB_ZLOG   = $(FINAL_PATH)/lib/libzlog.a
LIB_CRYPTO = $(FINAL_PATH)/lib/libcrypto.a
#STATIC_LIB += $(LIB_ZLOG) 
#STATIC_LIB += $(LIB_CRYPTO)

#LIBS= -lcrypto -lpthread -ldl -lm
LIBS= -lpthread -ldl -lm

DEMO_TARGET = demo-sip
DEMO_SRCS = sip_client.c sip_register.c main.c


sip:
	$(CC) -shared -fPIC $(SIP_CFLAGS) $(SRCS) -o $(TARGET) $(DEPEND_LIB)
#	$(CC) -shared -fPIC $(SIP_CFLAGS) $(WEC_LDFLAGS) $(SRCS) -o $(TARGET) $(STATIC_LIB) $(LIBS)
#	$(STRIP) $(TARGET) 
	$(CC) $(SIP_CFLAGS) $(DEMO_SRCS) -o $(DEMO_TARGET) $(DEPEND_LIB) $(LIBS)


clean:
	rm -f *.o 
	rm -f *.x 
	rm -f *.flat
	rm -f *.map
	rm -f temp
	rm -f *.img
	rm -f $(TARGET)	
	rm -f $(DEMO_TARGET)
	rm -f *.gdb
	rm -f *.bak

install:
	mkdir -p $(FINAL_PATH)/include/libsip/
	cp -RLf $(INC_SIP)/* $(FINAL_PATH)/include/libsip/
	cp -af $(TARGET)  $(FINAL_PATH)/lib
#	install -d $(FINAL_PATH)

