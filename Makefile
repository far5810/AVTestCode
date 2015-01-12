#
#

CROSS_COMPILE 	:= armv7l-timesys-linux-gnueabi-
CC				:= $(CROSS_COMPILE)gcc
LD				:= $(CROSS_COMPILE)ld
STRIP			:= $(CROSS_COMPILE)strip
AR				:= $(CROSS_COMPILE)ar
RANLIB			:= $(CROSS_COMPILE)ranlib

CFLAGG			:= -Wall -I/workspace/i_MX6QSABRELite/toolchain/include/gstreamer-0.10 \
					-I/workspace/i_MX6QSABRELite/toolchain/include/glib-2.0 \
					-I/workspace/i_MX6QSABRELite/toolchain/lib/glib-2.0/include \
					-I/workspace/i_MX6QSABRELite/toolchain/include/libxml2
CFLAGA			:= -Wall
CFLAGF			:= -Wall
LIBT			:= -lpthread
LIBA			:= -lasound -lm
LIBF			:= -lavcodec -lavformat -lavutil -lswscale -lswresample
LIBG			:= -lgstreamer-0.10 -lgobject-2.0 -lglib-2.0

TARGET1P		:= pcmplay
TARGET1C		:= pcmcapture
TARGET2			:= mp3play
TARGET3			:= f3play
TARGET4			:= video_cap
TARGET5			:= mvplay

TARGET	:= $(TARGET1P) $(TARGET1C) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5)

all:$(TARGET)
	@cp $(TARGET1P) /workspace/210-dev/porting/imx-rfs/home/far/
	@cp $(TARGET1C) /workspace/210-dev/porting/imx-rfs/home/far/
	@cp $(TARGET2) /workspace/210-dev/porting/imx-rfs/home/far/
	@cp $(TARGET3) /workspace/210-dev/porting/imx-rfs/home/far/
	@cp $(TARGET4) /workspace/210-dev/porting/imx-rfs/home/far/
	@cp $(TARGET5) /workspace/210-dev/porting/imx-rfs/home/far/

$(TARGET1P):$(TARGET1P).c
	$(CC) $^ -o $@  $(CFLAGA) $(LIBA)
$(TARGET1C):$(TARGET1C).c
	$(CC) $^ -o $@  $(CFLAGA) $(LIBA)
$(TARGET2):$(TARGET2).c
	$(CC) $^ -o $@  $(CFLAGG) $(LIBG)
$(TARGET3):$(TARGET3).c
	$(CC) $^ -o $@  $(CFLAGF) $(LIBF) $(LIBA)
$(TARGET4):$(TARGET4).c
	$(CC) $^ -o $@  $(CFLAGF) $(LIBF)
$(TARGET5):$(TARGET5).c
	$(CC) $^ -o $@  $(CFLAGF) $(LIBF) $(LIBT) $(LIBA)
	
clean:
	@rm -rf *.o $(TARGET)
