# Makefile for building AMC.
# NB! If scripting with minischeme is wanted, then MINISCHEME_HOME should be set.

LIBS = -lbe -lmedia -ldevice
OPT = -O -fno-default-inline
# DBG = -g -Wuninitialized -Wshadow

OBJS = amc.o \
       sound.o \
       edit-script.o \
       str.o \
       widgets.o \
       dump_wav.o \
       chords.o \
       ndist.o \
       midi-out.o \
       bitmaps.o \
       read-wav-data.o \
       midi-keyb.o

ifndef MINISCHEME_HOME

  all: set_attr amc

else

SCM_DIR = $(MINISCHEME_HOME)
all: amc-miniscm set_attr amc

amc-miniscm.o: amc-miniscm.cpp amc-scheme.h
    c++ -c -O $(DBG) -I$(SCM_DIR) $<

amc-miniscm: $(SCM_DIR)/miniscm.o $(SCM_DIR)/get_line.o amc-miniscm.o
    c++ $^ -lbe -o amc-miniscm

$(SCM_DIR)/%.o: $(SCM_DIR)/%.c $(SCM_DIR)/miniscm.h
    c++ -c -O $(DBG) -I$(SCM_DIR) -o $@ $<
 
endif

.SUFFIXES:

set_attr: set_attr.cpp
    c++ $< -lbe

amc: $(OBJS)
    c++ $^ $(LIBS) -o amc
    set_attr amc
#   xres -o amc amc.rsrc
#   mimeset -f amc

install:
    ln -sf $(PWD)/amc /boot/apps
    ln -sf $(PWD)/amc /boot/home/config/be/Applications
    
bitmaps.o: bitmaps.cpp
    c++ -c bitmaps.cpp

%.o: %.cpp
    c++ $(DBG) -c $(OPT) $<
 
str.o: str.h
widgets.o: widgets.h
dump_wav.o: dump_wav.h
sound.o: amc.h str.h widgets.h dump_wav.h colors.h midi-out.h read-wav-data.h sound.h midi-keyb.h
chords.o: amc.h str.h widgets.h chords.h
ndist.o: amc.h str.h widgets.h ndist.h
edit-script.o: amc.h
midi-out.o: colors.h midi-out.h
bitmaps.o: bitmaps.h
amc.o: amc.h str.h widgets.h dump_wav.h chords.h ndist.h sound.h midi-out.h \
       colors.h bitmaps.h edit-script.h amc-scheme.h midi-keyb.h
read-wav-data.o: read-wav-data.h
midi-keyb.o: midi-keyb.h
