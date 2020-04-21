CXXFLAGS=-Wall -O3 -g -Wextra -Wno-unused-parameter -D_FILE_OFFSET_BITS=64
OBJECTS=panelviewer.o
BINARIES=panelviewer

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=..
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread

# Imagemagic flags, only needed if actually compiled.
MAGICK_CXXFLAGS?=`GraphicsMagick++-config --cppflags --cxxflags`
MAGICK_LDFLAGS?=`GraphicsMagick++-config --ldflags --libs`
AV_CXXFLAGS=`pkg-config --cflags libavcodec libavformat libswscale libavutil`

all : $(BINARIES)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

panelviewer: panelviewer.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) panelviewer.o -o $@ $(LDFLAGS) $(MAGICK_LDFLAGS) `pkg-config --cflags --libs  libavcodec libavformat libswscale libavutil`

%.o : %.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<

panelviewer.o : panelviewer.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) $(MAGICK_CXXFLAGS) -c -o $@ $< -Wno-deprecated-declarations

# We're using a couple of deprecated functions. Pull request to update this to
# the latest libraries is welcome.
video-viewer.o: video-viewer.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -Wno-deprecated-declarations -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BINARIES) $(OPTIONAL_OBJECTS) $(OPTIONAL_BINARIES)

FORCE:
.PHONY: FORCE
