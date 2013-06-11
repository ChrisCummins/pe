noinst_PROGRAMS = fountains

fountains_SOURCES = fountains.c $(particle_engine_sources)

particle_engine_sources =		\
	fuzzy.c				\
	particle-engine.c		\
	$(NULL)

LDADD =					\
	$(COGL_LIBS)			\
	-lm

AM_CFLAGS =				\
	$(COGL_CFLAGS)			\
	-g				\
	-O0				\
	-Wall				\
	-Wall 				\
	-Wextra				\
	-Wstrict-prototypes		\
	-Wno-unused-parameter		\
	$(NULL)
