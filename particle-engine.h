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
	CoglTexture *texture;

	/* Maximum number of particles to generate */
	int max_particles;

	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglPipeline *pipeline;
	CoglPrimitive *primitive;

	/* Controls whether new particles are created. If false, no new
	 * particles are created */
	CoglBool source_active;

	int used_particles_count;
	CoglBool *used_particles;
	struct particle *particles;
	struct vertex *vertices;

	CoglAttributeBuffer *attribute_buffer;

	GRand *rand;

	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	int new_particles_per_ms;

	float min_initial_velocity[3];
	float max_initial_velocity[3];

	float min_initial_position[3];
	float max_initial_position[3];

	float point_size;
};

struct particle_engine *particle_engine_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_engine_free(struct particle_engine *engine);

void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H_ */
