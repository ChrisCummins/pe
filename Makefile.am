noinst_PROGRAMS = fireworks fountains snow catherine_wheel galaxy

fireworks_SOURCES = fireworks.c $(particle_emitter_sources)
fountains_SOURCES = fountains.c $(particle_emitter_sources)
snow_SOURCES = snow.c $(particle_emitter_sources)
catherine_wheel_SOURCES = catherine-wheel.c $(particle_emitter_sources)
galaxy_SOURCES = galaxy.c $(particle_swarm_sources)

particle_engine_sources = particle-engine.c

particle_emitter_sources =		\
	$(particle_engine_sources)	\
	fuzzy.c				\
	particle-emitter.c		\
	$(NULL)

particle_swarm_sources =		\
	$(particle_engine_sources)	\
	fuzzy.c				\
	particle-swarm.c		\
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
