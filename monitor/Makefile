CC = gcc -g -g

ifndef SMR_SSD_CACHE_DIR
SMR_SSD_CACHE_DIR = ../src
endif

CFLAGS += -Wall -pthread
CPPFLAGS += -I$(SMR_SSD_CACHE_DIR) 

RM = rm -rf

OBJS = global.o report.o timerUtils.o shmlib.o monitor.o

all: $(OBJS) monitor
	@echo 'Successfully built monitor...'

monitor:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJS) -lrt -o $@

global.o: $(SMR_SSD_CACHE_DIR)/global.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $?

report.o: $(SMR_SSD_CACHE_DIR)/report.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $?

shmlib.o: $(SMR_SSD_CACHE_DIR)/shmlib.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $?

timerUtils.o: $(SMR_SSD_CACHE_DIR)/timerUtils.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $?

monitor.c: ./monitor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $?

clean:
	$(RM) ./*.o
	$(RM) ./monitor
	
                     

