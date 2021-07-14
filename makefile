PROG = bzpwork
CXX = g++
LIB =
RM = rm -f
CFLAGS = -c -I. -O2 -Wall -D_7ZIP_ST
LDFLAGS = 

OBJS = \
  Alloc.o \
  LzFind.o \
  LzmaDec.o \
  LzmaEnc.o \
  main.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CXX) -o $(PROG) $(LDFLAGS) $(OBJS) $(LIB) 

main.o: main.c
	$(CXX) $(CFLAGS) main.c

Alloc.o: LZMA/Alloc.c
	$(CXX) $(CFLAGS) LZMA/Alloc.c

LzFind.o: LZMA/LzFind.c
	$(CXX) $(CFLAGS) LZMA/LzFind.c

LzmaDec.o: LZMA/LzmaDec.c
	$(CXX) $(CFLAGS) LZMA/LzmaDec.c

LzmaEnc.o: LZMA/LzmaEnc.c
	$(CXX) $(CFLAGS) LZMA/LzmaEnc.c

clean:
	-$(RM) $(PROG) $(OBJS)
