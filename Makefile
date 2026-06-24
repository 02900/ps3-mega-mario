#---------------------------------------------------------------------------------
# Mega Mario (PS3) - Makefile
#
# PS3 homebrew port of "mega-mario" (a C++/SFML ECS platformer). The original is
# already C++, so this builds .cpp with ppu-g++; the SFML layer is replaced by a
# PS3 backend. Branch `raylib-backend`: that backend draws with raylib (over RSXGL)
# instead of Tiny3D/ya2d. Build with the raylib toolchain image:
#   docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain-raylib make
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

#---------------------------------------------------------------------------------
# Application metadata
#---------------------------------------------------------------------------------
TITLE		:=	Mega Mario
APPID		:=	MEGAMARIO
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000
ICON0		:=	pkgfiles/ICON0.PNG
SFOXML		:=	sfo.xml

include $(PSL1GHT)/ppu_rules

#---------------------------------------------------------------------------------
# Directories
#
# raylib backend: the Clay menu is kept (Clay is layout-only) but rendered with
# raylib in source/clay_renderer_raylib.c, so extern/clay-ps3 is referenced only
# for clay.h / clay_renderer.h (headers) — its Tiny3D clay_renderer.c is NOT
# compiled (extern/clay-ps3 is in INCLUDES but not SOURCES). ttf_render is dropped.
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include extern/clay-ps3
PKGFILES	:=	pkgfiles

#---------------------------------------------------------------------------------
# Libraries to link: raylib over the RSXGL stack (sprites/window/input), plus
# MikMod (audio, independent of the GPU). RSXGL underneath is C++, so the link uses
# the C++ driver (LD is already $(CXX) since there are .cpp files).
#---------------------------------------------------------------------------------
LIBS		:=	-lraylib -lEGL -lGL \
			-lrsx -lgcm_sys -lio -lsysutil -lsysmodule \
			-lnet -lrt -llv2 -lpng -lz -lm \
			-lmikmod -laudio

#---------------------------------------------------------------------------------
# Compiler flags. C uses gnu99; C++ uses gnu++17 (ppu gcc 7.2 has no C++20, so the
# original C++20 source is built at C++17 — see todo/ROADMAP.md).
#---------------------------------------------------------------------------------
CFLAGS		=	-O2 -Wall -mcpu=cell -std=gnu99 -D__RSX__ $(MACHDEP) $(INCLUDE)
# -include ps3_compat.h: provide std::to_string / stoi / stof (missing on newlib).
CXXFLAGS	=	-O2 -Wall -mcpu=cell -std=gnu++17 -D__RSX__ -fno-exceptions -fno-rtti -include ps3_compat.h $(MACHDEP) $(INCLUDE)
LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# Debug logging (optional)
#---------------------------------------------------------------------------------
ifdef DEBUGLOG
CFLAGS		+=	-DENABLE_LOGGING
CXXFLAGS	+=	-DENABLE_LOGGING
LIBS		+=	-ldbglogger
endif

LIBDIRS	:=

#---------------------------------------------------------------------------------
# Build rules
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)
export BUILDDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))
PNGFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.png)))
JPGFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.jpg)))
MODFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.mod)))
S3MFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.s3m)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(addsuffix .o,$(PNGFILES)) \
			$(addsuffix .o,$(JPGFILES)) \
			$(addsuffix .o,$(MODFILES)) \
			$(addsuffix .o,$(S3MFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			$(LIBPSL1GHT_INC) \
			-I$(PORTLIBS)/include/freetype2 \
			-I$(CURDIR)/$(BUILD) -I$(PORTLIBS)/include

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
			$(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo Cleaning...
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self EBOOT.BIN *.pkg

run: $(BUILD)
	ps3load $(OUTPUT).self

pkg: $(BUILD) $(OUTPUT).pkg

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

%.bin.o: %.bin
	@echo $(notdir $<)
	@$(bin2o)

%.png.o: %.png
	@echo $(notdir $<)
	@$(bin2o)

%.jpg.o: %.jpg
	@echo $(notdir $<)
	@$(bin2o)

%.mod.o: %.mod
	@echo $(notdir $<)
	@$(bin2o)

%.s3m.o: %.s3m
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
