#ifndef _PARTICLE_ENGINE_H_
#define _PARTICLE_ENGINE_H_

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
