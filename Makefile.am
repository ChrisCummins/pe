noinst_PROGRAMS = fireworks fountains snow catherine_wheel

fireworks_SOURCES = fireworks.c $(particle_engine_sources)
fountains_SOURCES = fountains.c $(particle_engine_sources)
snow_SOURCES = snow.c $(particle_engine_sources)
catherine_wheel_SOURCES = catherine-wheel.c $(particle_engine_sources)

particle_engine_sources =		\
	fuzzy.c				\
	particle-emitter.c		\
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
