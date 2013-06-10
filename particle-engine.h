#ifndef _PARTICLE_ENGINE_H_
#define _PARTICLE_ENGINE_H_

#include "variance.h"

#include <cogl/cogl.h>

struct color {
	unsigned char r, g, b, a;
};

struct particle {
	float initial_position[3];
	float initial_velocity[3];
	struct color initial_color;

	/* Time of creation in milliseconds relative to the start of the engine
	 * when the particle was created */
	gdouble creation_time;

	/* The maximum age of this particle in msecs. The particle will linearly
	 * fade out until this age */
	gdouble max_age;
};

struct vertex {
	float position[3];
	struct color color;
};

struct particle_engine {

	/*
	 * Controls whether new particles are created. If false, no new
	 * particles are created.
	 */
	CoglBool source_active;

	/*
	 * The maximum number of particles that can exist at any given moment in
	 * time. When this number of particles has been generated, then new
	 * particles will only be created as and when old particles are
	 * destroyed.
	 */
	int particle_count;

	/*
	 * This controls the rate at which new particles are generated.
	 */
	int new_particles_per_ms;

	/*
	 * The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size.
	 */
	float particle_size;

	/*
	 * The time (in seconds) that a particle will exist for. After this
	 * amount of time, the particle will be destroyed, and a new particle
	 * will be created. The variance is a value in the rang [0, 1] which
	 * controls the amount of randomness used in determining this
	 * lifespan. For example, A lifespan variance of 0.0 means that each
	 * particle will have a uniform lifespan of particle_lifespan, and a
	 * variance of 0.5 means that each particle will have a lifespan of
	 * particle_lifespan ± 50%.
	 */
	struct double_variance particle_lifespan;

	float min_initial_velocity[3];
	float max_initial_velocity[3];

	float min_initial_position[3];
	float max_initial_position[3];

	/* <priv> */
	struct {
		CoglContext *ctx;
		CoglFramebuffer *fb;
		CoglPipeline *pipeline;
		CoglPrimitive *primitive;
		CoglTexture *texture;
		CoglAttributeBuffer *attribute_buffer;

		GRand *rand;

		int used_particles_count;
		CoglBool *used_particles;
		struct particle *particles;
		struct vertex *vertices;

		GTimer *timer;
		gdouble current_time;
		gdouble last_update_time;
	} priv;
};

struct particle_engine *particle_engine_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_engine_free(struct particle_engine *engine);

void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H_ */
