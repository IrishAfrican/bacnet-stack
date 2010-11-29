# Main Makefile for BACnet-stack project with GCC

# tools - only if you need them.
# Most platforms have this already defined
# CC = gcc
# AR = ar

# configuration
BACNET_DEFINES = -DPRINT_ENABLED=1 -DBACAPP_ALL -DBACFILE
# un-comment the next line to build the routing demo application
#BACNET_DEFINES += -DBAC_ROUTING

#BACDL_DEFINE=-DBACDL_ETHERNET=1
#BACDL_DEFINE=-DBACDL_ARCNET=1
#BACDL_DEFINE=-DBACDL_MSTP=1
BACDL_DEFINE=-DBACDL_BIP=1

DEFINES = $(BACNET_DEFINES) $(BACDL_DEFINE) -DWEAK_FUNC=

# directories
BACNET_PORT = linux
BACNET_PORT_DIR = ../ports/${BACNET_PORT}

BACNET_OBJECT = ../demo/object
BACNET_HANDLER = ../demo/handler
BACNET_CORE = ../src
BACNET_INCLUDE = ../include
# compiler configuration
#STANDARDS = -std=c99
INCLUDES = -I$(BACNET_INCLUDE) -I$(BACNET_PORT_DIR) -I$(BACNET_OBJECT) -I$(BACNET_HANDLER)
OPTIMIZATION = -Os
DEBUGGING =
WARNINGS = -Wall -Wmissing-prototypes
ifeq (${BUILD},debug)
OPTIMIZATION = -O0
DEBUGGING = -g -DDEBUG_ENABLED=1
ifeq (${BACDL_DEFINE},-DBACDL_BIP=1)
DEFINES += -DBIP_DEBUG
endif
endif
CFLAGS  = $(WARNINGS) $(DEBUGGING) $(OPTIMIZATION) $(STANDARDS) $(INCLUDES) $(DEFINES)

.EXPORT_ALL_VARIABLES:

all: library demos
.PHONY : all library demos clean

library:
	$(MAKE) -C lib all

demos:
	$(MAKE) -C demo all

# Add "ports" to the build, if desired
ports:	atmega168 bdk-atxx4-mstp at91sam7s
	@echo "Built the ARM7 and AVR ports"

atmega168: ports/atmega168/Makefile
	$(MAKE) -C ports/atmega168 clean all

at91sam7s: ports/at91sam7s/makefile
	$(MAKE) -C ports/at91sam7s clean all

bdk-atxx4-mstp: ports/bdk-atxx4-mstp/Makefile
	$(MAKE) -C ports/bdk-atxx4-mstp clean all

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C demo clean


