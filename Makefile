# Makefile for GBA development

# DevkitPro paths
DEVKITPRO  = C:/devkitPro
DEVKITARM  = $(DEVKITPRO)/devkitARM
LIBGBA     = $(DEVKITPRO)/libgba

# Program name
TARGET = heavy_rain_ai

# Compiler settings
CC      = $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY = $(DEVKITARM)/bin/arm-none-eabi-objcopy
GBAFIX  = $(DEVKITPRO)/tools/bin/gbafix

# Compiler flags
CFLAGS = -mthumb-interwork -mthumb \
         -O2 -Wall \
         -mcpu=arm7tdmi -mtune=arm7tdmi \
         -fomit-frame-pointer -ffast-math \
         -I$(DEVKITARM)/include \
         -I$(LIBGBA)/include

# Linker flags
LDFLAGS = -mthumb-interwork -mthumb \
          -L$(DEVKITARM)/lib \
          -L$(LIBGBA)/lib \
          -specs=gba.specs \
          -lgba

# Object files
OFILES = $(TARGET).o

# Build rules
.PHONY: all clean

all: $(TARGET).gba

# Compile source files
$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files
$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) -o $@

# Create GBA ROM
$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -v -O binary $< $@
	$(GBAFIX) $@

# Clean build files
clean:
	rm -f $(OFILES) $(TARGET).elf $(TARGET).gba