#ifndef _PARTICLE_ENGINE_H_
#define _PARTICLE_ENGINE_H_

#include <cogl/cogl.h>

/* Particle state flags: */
#define PARTICLE_STATE_CLEAN          0
#define PARTICLE_STATE_DIRTY_POSITION 1 << 0
#define PARTICLE_STATE_DIRTY_COLOR    1 << 1

/*
 * The particle engine is an opaque data structure
 */
struct particle_engine;

/*
 * A representation of a single particle.
 */
struct particle {
	float position[3];
	float velocity[3];
	CoglColor color;

	/* The maximum age of this particle in seconds. The particle will linearly
	 * fade out until this age */
	gdouble max_age;

	/* Time to live. This value represents the age of the particle. When it
	 * reaches zero, the particle ist destroyed. */
	gdouble ttl;
};

/*
 * Create and return a new particle engine.
 */
struct particle_engine *particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb,
					    int particle_count,
					    float particle_size);

/*
 * Destroy a particle engine and free associated resources.
 */
void particle_engine_free(struct particle_engine *engine);

/*
 * Return the particle at the given index.
 */
inline struct particle *particle_engine_get_particle(struct particle_engine *engine,
						     int index);

/*
 * Mark a particle as dirty, queuing it to be redrawn on next paint.
 */
inline void particle_engine_set_particle_state(struct particle_engine *engine,
					       int index,
					       unsigned char state_flag);

/*
 * Paint function.
 */
void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H */
