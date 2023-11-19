
#$(info CC=${CC})

ifneq (${CROSS_COMPILE},)
CC ?= ${CROSS_COMPILE}gcc
else
CC ?= gcc
endif

APP_NAME=aesdsocket
CFLAGS=-I. -lpthread -lrt

USE_AESD_CHAR_DEVICE = 0

ifeq (${USE_AESD_CHAR_DEVICE}, 1)
CFLAGS+= -DUSE_AESD_CHAR_DEVICE
endif


OBJ = aesdsocket.o 

all: $(APP_NAME)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP_NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


clean:
	@rm -f *.o
	@rm -f $(APP_NAME)
