TARGET_NAME       := chimerasnes
DEBUG              = 0
GIT_VERSION       := " $(shell git rev-parse --short HEAD)"
STATIC_LINKING     = 0
ROOT_DIR          := $(shell pwd)
CORE_DIR          := $(ROOT_DIR)/source
LIBRETRO_COMM_DIR  = $(ROOT_DIR)/libretro-common
BUILD_DIR          = $(LIBRETRO_COMM_DIR)/build
OBJOUT             = -o

ifeq ($(platform),android_arm64-v8a)
    include $(BUILD_DIR)/Makefile.android_arm64-v8a
else ifeq ($(platform),android_armeabi)
    include $(BUILD_DIR)/Makefile.android_armeabi
else ifeq ($(platform),android_armeabi-v7a)
    include $(BUILD_DIR)/Makefile.android_armeabi-v7a
else ifeq ($(platform),android_mips)
    include $(BUILD_DIR)/Makefile.android_mips
else ifeq ($(platform),android_mips64)
    include $(BUILD_DIR)/Makefile.android_mips64
else ifeq ($(platform),android_x86)
    include $(BUILD_DIR)/Makefile.android_x86
else ifeq ($(platform),android_x86_64)
    include $(BUILD_DIR)/Makefile.android_x86_64
else ifeq ($(platform),linux-portable_x86)
    include $(BUILD_DIR)/Makefile.linux-portable_x86
else ifeq ($(platform),linux-portable_x86_64)
    include $(BUILD_DIR)/Makefile.linux-portable_x86_64
else ifeq ($(platform),linux_x86)
    include $(BUILD_DIR)/Makefile.linux_x86
else ifeq ($(platform),linux_x86_64)
    include $(BUILD_DIR)/Makefile.linux_x86_64
else ifeq ($(platform),mingw_x86)
    include $(BUILD_DIR)/Makefile.mingw_x86
else ifeq ($(platform),mingw_x86_64)
    include $(BUILD_DIR)/Makefile.mingw_x86_64
else ifeq ($(platform),wii_ppc)
    include $(BUILD_DIR)/Makefile.wii_ppc
else ifeq ($(platform),windows_x86)
    include $(BUILD_DIR)/Makefile.windows_x86
else ifeq ($(platform),windows_msvc2003_x86)
    include $(BUILD_DIR)/Makefile.windows_msvc2003_x86
else ifeq ($(platform),windows_msvc2005_x86)
    include $(BUILD_DIR)/Makefile.windows_msvc2005_x86
else ifeq ($(platform),windows_msvc2008_x86)
    include $(BUILD_DIR)/Makefile.windows_msvc2008_x86
else ifeq ($(platform),windows_msvc2010_x86)
    include $(BUILD_DIR)/Makefile.windows_msvc2010_x86
else ifeq ($(platform),windows_msvc2010_x64)
    include $(BUILD_DIR)/Makefile.windows_msvc2010_x64
else ifeq ($(platform),windows_msvc2015_x64)
    include $(BUILD_DIR)/Makefile.windows_msvc2015_x64
else ifeq ($(platform),windows_x86_64)
    include $(BUILD_DIR)/Makefile.windows_x86_64
else
    ifeq ($(platform),)
        ifeq ($(shell uname -s),)
            platform = win
        else ifneq ($(findstring MINGW,$(shell uname -s)),)
            platform = win
        else ifneq ($(findstring Darwin,$(shell uname -s)),)
            platform = osx
        else ifneq ($(findstring win,$(shell uname -s)),)
            platform = win
        else
            platform = unix
        endif
    endif

    BLANK           :=
    SPACE           := $(BLANK) $(BLANK)
    BACKSLASH       := \$(BLANK)
    filter_out1      = $(filter-out $(firstword $1),$1)
    filter_out2      = $(call filter_out1,$(call filter_out1,$1))
    unixpath         = $(subst \,/,$1)
    unixcygpath      = /$(subst :,,$(call unixpath,$1))
    system_platform  = unix

    ifeq ($(shell uname -a),)
        EXE_EXT = .exe
        system_platform = win
    else ifneq ($(findstring Darwin,$(shell uname -a)),)
        system_platform = osx
        arch = intel

        ifeq ($(shell uname -p),arm64)
            arch = arm
        endif

        ifeq ($(shell uname -p),powerpc)
            arch = ppc
        endif
    else ifneq ($(findstring MINGW,$(shell uname -a)),)
        system_platform = win
    endif

    LIBM = -lm

    ifeq ($(STATIC_LINKING),1)
        EXT=a

        ifeq ($(platform), unix)
            PLAT=_unix
        endif
    endif

    ifeq ($(platform), unix) # UNIX
        EXT    ?= so
        TARGET := $(TARGET_NAME)_libretro$(PLAT).$(EXT)
        fpic   := -fPIC
        LTO     = -flto=4 -fuse-linker-plugin
        SHARED := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined

        ifneq ($(findstring Haiku,$(shell uname -a)),)
            LIBM :=
        endif
    else ifeq ($(platform), linux-portable) # Linux (portable library)
        EXT    ?= so
        TARGET := $(TARGET_NAME)_libretro.$(EXT)
        fpic   := -fPIC -nostdlib
        SHARED := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        LIBM   :=
    else ifeq ($(platform), osx) # OS X
        EXT              ?= dylib
        TARGET           := $(TARGET_NAME)_libretro.$(EXT)
        fpic             := -fPIC

        ifeq ($(arch),ppc)
            CFLAGS += -D__ppc__ -DMSB_FIRST -DHAVE_NO_LANGEXTRA
        endif

        OSXVER            = $(shell sw_vers -productVersion | cut -d. -f 2)
        OSX_LT_MAVERICKS  = `(( $(OSXVER) <= 9)) && echo "YES"`

        ifeq ($(OSX_LT_MAVERICKS),YES)
            fpic += -mmacosx-version-min=10.1
        endif

        SHARED := -dynamiclib

        ifeq ($(CROSS_COMPILE),1)
            TARGET_RULE  = -target $(LIBRETRO_APPLE_PLATFORM) -isysroot $(LIBRETRO_APPLE_ISYSROOT)
            CFLAGS      += $(TARGET_RULE)
            CPPFLAGS    += $(TARGET_RULE)
            LDFLAGS     += $(TARGET_RULE)
        endif
    else ifneq (,$(findstring ios,$(platform))) # iOS
        EXT    ?= dylib
        TARGET := $(TARGET_NAME)_libretro_ios.$(EXT)
        fpic   := -fPIC
        SHARED := -dynamiclib

        ifeq ($(IOSSDK),)
            IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
        endif

        ifeq ($(platform),ios-arm64)
            CC = cc -arch arm64 -isysroot $(IOSSDK)
        else
            CC = cc -arch armv7 -isysroot $(IOSSDK)
        endif

        LD      = armv7-apple-darwin11-ld
        CFLAGS += -DIOS

        ifeq ($(platform),$(filter $(platform),ios9 ios-arm64))
            CC     += -miphoneos-version-min=8.0
            CFLAGS += -miphoneos-version-min=8.0
        else
            CC     += -miphoneos-version-min=5.0
            CFLAGS += -miphoneos-version-min=5.0
        endif
    else ifeq ($(platform), tvos-arm64) # tvOS
        EXT    ?= dylib
        TARGET := $(TARGET_NAME)_libretro_tvos.$(EXT)
        fpic   := -fPIC
        SHARED := -dynamiclib

        ifeq ($(IOSSDK),)
            IOSSDK := $(shell xcodebuild -version -sdk appletvos Path)
        endif

        CFLAGS += -DIOS
        CC      = cc -arch arm64 -isysroot $(IOSSDK)
    else ifeq ($(platform), theos_ios) # Theos iOS
        DEPLOYMENT_IOSVERSION               = 5.0
        TARGET                              = iphone:latest:$(DEPLOYMENT_IOSVERSION)
        ARCHS                               = armv7 armv7s
        TARGET_IPHONEOS_DEPLOYMENT_VERSION  = $(DEPLOYMENT_IOSVERSION)
        THEOS_BUILD_DIR                    := objs
        include $(THEOS)/makefiles/common.mk
        LIBRARY_NAME                        = $(TARGET_NAME)_libretro_ios
    else ifeq ($(platform), qnx) # QNX
        EXT    ?= so
        TARGET := $(TARGET_NAME)_libretro_qnx.$(EXT)
        fpic   := -fPIC
        SHARED := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CC      = qcc -Vgcc_ntoarmv7le
        AR      = qcc -Vgcc_ntoarmv7le
        CFLAGS += -D__BLACKBERRY_QNX__
    else ifneq (,$(filter $(platform), ps3 psl1ght)) # Lightweight PS3 Homebrew SDK
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = $(PS3DEV)/ppu/bin/ppu-$(COMMONLV)gcc$(EXE_EXT)
        AR              = $(PS3DEV)/ppu/bin/ppu-$(COMMONLV)ar$(EXE_EXT)
        CFLAGS         += -D__ppc__ -DMSB_FIRST -D__PS3__
        STATIC_LINKING  = 1

        ifeq ($(platform), psl1ght)
            CFLAGS += -D__PSL1GHT__
        endif
    else ifeq ($(platform), psp1) # PSP1
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = psp-gcc$(EXE_EXT)
        AR              = psp-ar$(EXE_EXT)
        CFLAGS         += -DPSP -D_PSP_FW_VERSION=371 -I$(shell psp-config --pspsdk-path)/include
        CFLAGS         += -G0 -march=allegrex -mno-abicalls -fno-pic
        STATIC_LINKING  = 1
    else ifeq ($(platform), vita) # Vita
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = arm-vita-eabi-gcc$(EXE_EXT)
        AR              = arm-vita-eabi-ar$(EXE_EXT)
        CFLAGS         += -DVITA
        STATIC_LINKING  = 1
    else ifeq ($(platform), ctr) # CTR (3DS)
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = $(DEVKITARM)/bin/arm-none-eabi-gcc$(EXE_EXT)
        AR              = $(DEVKITARM)/bin/arm-none-eabi-ar$(EXE_EXT)
        CFLAGS         += -DARM11 -D_3DS
        CFLAGS         += -march=armv6k -mtune=mpcore -mfloat-abi=hard
        CFLAGS         += -Wall -mword-relocations
        CFLAGS         += -D_3DS
        STATIC_LINKING  = 1
    else ifeq ($(platform), rpi2) # Raspberry Pi 2
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = arm-linux-gnueabihf-gcc$(EXE_EXT)
        AR         = arm-linux-gnueabihf-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS    += -DARM
        CFLAGS    += -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -funsafe-math-optimizations
        HAVE_NEON  = 1
    else ifeq ($(platform), rpi3) # Raspberry Pi 3
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = arm-linux-gnueabihf-gcc$(EXE_EXT)
        AR         = arm-linux-gnueabihf-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS    += -DARM
        CFLAGS    += -marm -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -funsafe-math-optimizations
        HAVE_NEON  = 1
    else ifeq ($(platform), rpi3_64) # Raspberry Pi 3 (64-bit)
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = aarch64-linux-gnu-gcc$(EXE_EXT)
        AR         = aarch64-linux-gnu-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS    += -DARM
        CFLAGS    += -mcpu=cortex-a53 -mtune=cortex-a53 -funsafe-math-optimizations
        HAVE_NEON  = 1
    else ifeq ($(platform), rpi4) # Raspberry Pi 4
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = arm-linux-gnueabihf-gcc$(EXE_EXT)
        AR         = arm-linux-gnueabihf-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS    += -DARM
        CFLAGS    += -marm -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard -funsafe-math-optimizations
        HAVE_NEON  = 1
    else ifeq ($(platform), rpi4_64) # Raspberry Pi 4 (64-bit)
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = aarch64-linux-gnu-gcc$(EXE_EXT)
        AR         = aarch64-linux-gnu-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS    += -DARM
        CFLAGS    += -mcpu=cortex-a72 -mtune=cortex-a72 -funsafe-math-optimizations
        HAVE_NEON  = 1
    else ifneq (,$(findstring CortexA73_G12B,$(platform))) # ODROIDN2
        TARGET    := $(TARGET_NAME)_libretro.so
        CC         = aarch64-linux-gnu-gcc$(EXE_EXT)
        AR         = aarch64-linux-gnu-ar$(EXE_EXT)
        fpic      := -fPIC
        SHARED    := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        ARCH       = arm64
        CFLAGS    += -DARM -funsafe-math-optimizations
        CFLAGS    += -march=armv8-a+crc -mcpu=cortex-a73 -mtune=cortex-a73.cortex-a53
        HAVE_NEON  = 1
    else ifeq ($(platform), classic_armv7_a7) # NESC, SNESC, C64 mini
        TARGET      := $(TARGET_NAME)_libretro.so
        CC           = arm-linux-gnueabihf-gcc$(EXE_EXT)
        AR           = arm-linux-gnueabihf-ar$(EXE_EXT)
        fpic        := -fPIC
        SHARED      := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS      += -falign-functions=1 -falign-jumps=1 -falign-loops=1 -fno-unroll-loops \
                       -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
        CPPFLAGS    += $(CFLAGS)
        ASFLAGS     += $(CFLAGS)
        HAVE_NEON    = 1
        ARCH         = arm
        BUILTIN_GPU  = neon

        ifeq ($(shell echo `$(CC) -dumpversion` "< 4.9" | bc -l), 1)
            CFLAGS += -march=armv7-a
        else
            CFLAGS += -march=armv7ve

            ifeq ($(shell echo `$(CC) -dumpversion` ">= 5" | bc -l), 1) # If gcc is 5.0 or later
                LDFLAGS += -static-libgcc -static-libstdc++
            endif
        endif
    else ifeq ($(platform), classic_armv8_a35) # Odroid Go Advance
        TARGET      := $(TARGET_NAME)_libretro.so
        CC           = arm-linux-gnueabihf-gcc$(EXE_EXT)
        AR           = arm-linux-gnueabihf-ar$(EXE_EXT)
        fpic        := -fPIC
        SHARED      := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS      += -falign-functions=1 -falign-jumps=1 -falign-loops=1 -fno-unroll-loops \
                       -marm -mcpu=cortex-a35 -mfpu=neon-fp-armv8 -mfloat-abi=hard
        CPPFLAGS    += $(CFLAGS)
        ASFLAGS     += $(CFLAGS)
        HAVE_NEON    = 1
        ARCH         = arm
        BUILTIN_GPU  = neon
    else ifeq ($(platform), xenon) # Xbox 360
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_xenon360.$(EXT)
        CC              = xenon-gcc$(EXE_EXT)
        AR              = xenon-ar$(EXE_EXT)
        CFLAGS         += -D__LIBXENON__ -m32 -D__ppc__
        STATIC_LINKING  = 1
    else ifeq ($(platform), ngc) # Nintendo Game Cube
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
        AR              = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
        CFLAGS         += -DGEKKO -DHW_DOL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -DMSB_FIRST
        CFLAGS         += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
        STATIC_LINKING  = 1
    else ifeq ($(platform), wii) # Nintendo Wii
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
        AR              = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
        CFLAGS         += -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__ -DMSB_FIRST
        CFLAGS         += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
        STATIC_LINKING  = 1
    else ifeq ($(platform), wiiu) # Nintendo WiiU
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        CC              = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
        AR              = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
        CFLAGS         += -DGEKKO -DHW_RVL -mwup -mcpu=750 -meabi -mhard-float -D__ppc__ -DMSB_FIRST
        CFLAGS         += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
        STATIC_LINKING  = 1
    else ifneq (,$(findstring armv,$(platform))) # ARM
        EXT    ?= so
        TARGET := $(TARGET_NAME)_libretro.$(EXT)
        fpic   := -fPIC
        SHARED := -shared -Wl,--no-undefined
        CC     ?= gcc
        CFLAGS += -marm -DARM

        ifneq (,$(findstring cortexa8,$(platform)))
            CFLAGS  += -mcpu=cortex-a8
            ASFLAGS += -mcpu=cortex-a8
        else ifneq (,$(findstring cortexa9,$(platform)))
            CFLAGS  += -mcpu=cortex-a9
            ASFLAGS += -mcpu=cortex-a9
        endif

        ifneq (,$(findstring neon,$(platform)))
            CFLAGS    += -mfpu=neon
            ASFLAGS   += -mfpu=neon
            HAVE_NEON  = 1
        endif

        ifneq (,$(findstring softfloat,$(platform)))
            CFLAGS  += -mfloat-abi=softfp
            ASFLAGS += -mfloat-abi=softfp
        else ifneq (,$(findstring hardfloat,$(platform)))
            CFLAGS  += -mfloat-abi=hard
            ASFLAGS += -mfloat-abi=hard
        endif
    else ifeq ($(platform), emscripten) # Emscripten
        TARGET         := $(TARGET_NAME)_libretro_emscripten.bc
        STATIC_LINKING  = 1
    else ifeq ($(platform), gcw0) # GCW0
        TARGET := $(TARGET_NAME)_libretro.so
        CC      = /opt/gcw0-toolchain/usr/bin/mipsel-linux-gcc
        AR      = /opt/gcw0-toolchain/usr/bin/mipsel-linux-ar
        fpic   := -fPIC -nostdlib
        SHARED := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS += $(PTHREAD_FLAGS)
        CFLAGS += -march=mips32 -mtune=mips32r2 -mhard-float
    else ifeq ($(platform), miyoo) # MIYOO
        TARGET := $(TARGET_NAME)_libretro.so
        CC      = /opt/miyoo/usr/bin/arm-linux-gcc
        AR      = /opt/miyoo/usr/bin/arm-linux-ar
        fpic   := -fPIC -nostdlib
        SHARED := -shared -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        LIBM   :=
        CFLAGS += -march=armv5te -mtune=arm926ej-s
        CFLAGS += -fno-unroll-loops
    else ifeq ($(platform), msvc)
        OBJOUT = -Fo
    else ifeq ($(platform), switch) # Nintendo Switch (libtransistor)
        EXT     = a
        TARGET := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        include $(LIBTRANSISTOR_HOME)/libtransistor.mk
        STATIC_LINKING = 1
    else ifeq ($(platform), libnx) # Nintendo Switch (libnx)
        include $(DEVKITPRO)/libnx/switch_rules
        EXT             = a
        TARGET         := $(TARGET_NAME)_libretro_$(platform).$(EXT)
        DEFINES        := -DSWITCH=1 -U__linux__ -U__linux -DRARCH_INTERNAL
        CFLAGS         := $(DEFINES) -g -O3 -fPIE -I$(LIBNX)/include/ -ftls-model=local-exec \
                          -Wl,--allow-multiple-definition -specs=$(LIBNX)/switch.specs
        CFLAGS         += $(INCDIRS)
        CFLAGS         += $(INCLUDE)  -D__SWITCH__ -DARM -march=armv8-a -mtune=cortex-a57 -mtp=soft
        CFLAGS         += -std=gnu11
        STATIC_LINKING  = 1
    else ifeq ($(platform), xbox1_msvc2003) # Windows MSVC 2003 Xbox 1
        TARGET         := $(TARGET_NAME)_libretro_xdk1.lib
        CC              = CL.exe
        LD              = lib.exe
        export INCLUDE := $(XDK)\xbox\include
        export LIB     := $(XDK)\xbox\lib
        PATH           := $(call unixcygpath,$(XDK)/xbox/bin/vc71):$(PATH)
        PSS_STYLE      := 2
        CFLAGS         += -D_XBOX -D_XBOX1
        STATIC_LINKING  = 1
    else # Windows
        EXT    ?= dll
        TARGET := $(TARGET_NAME)_libretro.$(EXT)
        CC     ?= gcc
        SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=libretro-common/link.T -Wl,--no-undefined
        CFLAGS += -D__WIN32__ -D__WIN32_LIBRETRO__
    endif

    ifeq ($(STATIC_LINKING),1)
        fpic=
        SHARED=
    endif

    include Makefile.common

    OBJECTS := $(SOURCES_C:.c=.o)
    DEFINES  = $(COREDEFINES)

    ifeq ($(STATIC_LINKING),1)
        DEFINES += -DSTATIC_LINKING
        FLTO     =
    endif

    ifeq ($(DEBUG), 1)
        WARNINGS_DEFINES =
        CODE_DEFINES     = -O0 -g
    else ifeq ($(platform), ios)
        WARNINGS_DEFINES = -Wall
        CODE_DEFINES     = -Ofast -DNDEBUG
    else ifneq (,$(findstring msvc,$(platform)))
        WARNINGS_DEFINES =
        CODE_DEFINES     = -O2 -DNDEBUG
    else
        WARNINGS_DEFINES = \
            -Wall \
            -Wextra \
            -pedantic \
            -Wno-sign-compare \
            -Wno-unused-parameter

        CODE_DEFINES = \
            -Ofast -DNDEBUG -fomit-frame-pointer \
            -fno-stack-protector -fvisibility=protected \
            -fno-ident -fmerge-all-constants -ffast-math \
            -fno-math-errno -fno-exceptions -fno-unwind-tables \
            -fno-asynchronous-unwind-tables -fdata-sections \
            -ffunction-sections -Wl,--gc-sections
    endif

    COMMON_DEFINES += $(CODE_DEFINES) $(WARNINGS_DEFINES) $(fpic) $(LTO)
    LDFLAGS        += $(LTO)
    CFLAGS         += $(DEFINES) $(COMMON_DEFINES) -DGIT_VERSION=\"$(GIT_VERSION)\"
    OBJOUT          = -o
    LINKOUT         = -o

    ifneq (,$(findstring msvc,$(platform)))
        OBJOUT = -Fo
        LINKOUT = -out:

        ifeq ($(STATIC_LINKING),1)
            LD             ?= lib.exe
            STATIC_LINKING  = 0
        else
            LD = link.exe
        endif
    else
        LD = $(CC)
    endif

    ifeq ($(platform), theos_ios)
        COMMON_FLAGS := -DIOS -DARM $(COMMON_DEFINES) $(INCFLAGS) -I$(THEOS_INCLUDE_PATH) -Wno-error
        $(LIBRARY_NAME)_CFLAGS += $(COMMON_FLAGS)
        ${LIBRARY_NAME}_FILES = $(SOURCES_C)
        include $(THEOS_MAKE_PATH)/library.mk
    else
        all: $(TARGET)
        $(TARGET): $(OBJECTS)
            ifeq ($(STATIC_LINKING), 1)
		$(AR) rcs $@ $(OBJECTS)
            else
		$(LD) $(LINKOUT)$@ $(SHARED) $(OBJECTS) $(LDFLAGS) $(LIBS) $(LIBM)
            endif

        %.o: %.c
		$(CC) -c $(OBJOUT)$@ $< $(CFLAGS) $(INCFLAGS)

        clean-objs:
		rm -rf $(OBJECTS)

        clean:
		rm -f $(OBJECTS) $(TARGET)

        .PHONY: clean
    endif
endif
