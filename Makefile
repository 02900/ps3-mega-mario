#---------------------------------------------------------------------------------
# Mega Mario (PS3) - Makefile
#
# PS3 homebrew port of "mega-mario" (a C++/SFML ECS platformer). The original is
# already C++, so this builds .cpp with ppu-g++; the SFML layer is replaced by a
# PS3 backend (ya2d / pad / MikMod / Clay). Build with the Docker toolchain:
#   docker run --rm -v "$PWD":/src -w /src ghcr.io/02900/ps3-toolchain make
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
# The extern/clay-ps3 submodule is checked in, but NOT compiled yet: its
# clay_renderer.c needs the ttf_render helper. Phase 1 (todo/ROADMAP.md) vendors
# ttf_render.{c,h} and re-adds `extern/clay-ps3` to SOURCES / INCLUDES.
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
PKGFILES	:=	pkgfiles

#---------------------------------------------------------------------------------
# Libraries to link (2D sprites, fonts, audio, image decode, net, UI)
#---------------------------------------------------------------------------------
LIBS		:=	-lya2d -lfont3d -ltiny3d -lsimdmath \
			-lgcm_sys -lrsx -lio -lsysutil -lrt -llv2 \
			-lpngdec -ljpgdec -lsysmodule -lm -lsysfs \
			-lnet -lfreetype -lz -lmikmod -laudio \
			-lmini18n -lnetctl

#---------------------------------------------------------------------------------
# Compiler flags. C uses gnu99; C++ uses gnu++17 (ppu gcc 7.2 has no C++20, so the
# original C++20 source is built at C++17 — see todo/ROADMAP.md).
#---------------------------------------------------------------------------------
CFLAGS		=	-O2 -Wall -mcpu=cell -std=gnu99 $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	-O2 -Wall -mcpu=cell -std=gnu++17 -fno-exceptions -fno-rtti $(MACHDEP) $(INCLUDE)
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
