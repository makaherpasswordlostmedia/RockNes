#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#
# rocknes - NES emulator plugin, ported from
# https://github.com/ObaraEmmanuel/NES (MIT licensed)

ROCKNES_SRCDIR = $(APPSDIR)/plugins/rocknes
ROCKNES_OBJDIR = $(BUILDDIR)/apps/plugins/rocknes

ROCKNES_SRC := $(call preprocess, $(ROCKNES_SRCDIR)/SOURCES)
ROCKNES_OBJ := $(call c2obj, $(ROCKNES_SRC))

OTHER_SRC += $(ROCKNES_SRC)

ROCKS += $(ROCKNES_OBJDIR)/rocknes.rock

$(ROCKNES_OBJDIR)/rocknes.rock: $(ROCKNES_OBJ)
