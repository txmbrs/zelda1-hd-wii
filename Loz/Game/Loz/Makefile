#---------------------------------------------------------------------------------
# Zelda 1 HD Wii Edition Makefile
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitpro")
endif

include $(DEVKITPPC)/wii_rules

TARGET      := boot
BUILD       := build
SOURCES     := .

CFLAGS      := -O2 -Wall -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float \
               -I. -I$(DEVKITPRO)/portlibs/wii/include -I$(DEVKITPRO)/libogc/include

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17 -MMD -MP

LIBDIRS     := -L$(DEVKITPRO)/portlibs/wii/lib -L$(DEVKITPRO)/portlibs/ppc/lib \
               -L$(DEVKITPRO)/libogc/lib/wii

LIBS        := -lSDL2main -lSDL2_mixer -lmodplug -lopusfile -lopus -lvorbisfile -lvorbis -logg \
               -lSDL2_image -lSDL2 -laesnd -lasnd -lwiikeyboard -lfat -lwiiuse -lbte \
               -lpng -ljpeg -lz -logc -lm

CPPFILES    := Player.cpp Monsters.cpp Object.cpp ItemObj.cpp World.cpp \
               TextBox.cpp SpriteAnimator.cpp Profile.cpp SaveFolder.cpp \
               StatusBar.cpp Submenu.cpp EliminateMenu.cpp GameMenu.cpp \
               RegisterMenu.cpp Credits.cpp Util.cpp Common.cpp \
               Input_wii.cpp Sound_wii.cpp Graphics_wii.cpp Loz_wii.cpp

OFILES      := $(addprefix $(BUILD)/,$(CPPFILES:.cpp=.o))

.PHONY: all clean

all: $(TARGET).dol

$(TARGET).dol: $(TARGET).elf align_dol.py
	@echo "Converting $< to $@ ..."
	@$(DEVKITPRO)/tools/bin/elf2dol $< $@
	@python3 align_dol.py $@
	@echo "Build successful: $@"

LDFLAGS     := -mrvl

$(TARGET).elf: $(OFILES)
	@echo "Linking $@ ..."
	@$(CXX) $(LDFLAGS) $(OFILES) $(LIBDIRS) $(LIBS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol
	@echo "Clean complete."

-include $(OFILES:.o=.d)
