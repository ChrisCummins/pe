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
	GArray *colors;

	/* Maximum number of particles to generate */
	int max_particles;

	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglPipeline *pipeline;
	CoglTexture *trexture;
	CoglPrimitive *primitive;

	int used_particles_count;
	CoglBool *used_particles;
	struct particle *particles;
	struct vertex *vertices;

	CoglAttributeBuffer *attribute_buffer;

	/* The unused particles are linked together in a linked list so we
	 * can quickly find the next free one. The particle structure is
	 * reused as a list node to avoid allocating anything */
	struct particle *next_unused_particle;

	GRand *rand;

	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;
	gdouble next_particle_time;

	float min_initial_velocity[3];
	float max_initial_velocity[3];

	float min_initial_position[3];
	float max_initial_position[3];

	float point_size;
};

struct particle_engine *particle_engine_new(CoglContext *ctx, CoglFramebuffer *fb);
void particle_engine_free(struct particle_engine *engine);

/**
 * particle_engine_set_time:
 * @engine: A pointer to the particle engine
 * @msecs: The time in milliseconds
 *
 * Sets the relative time in milliseconds at which to draw the
 * animation for this particle engine. The time does need to be
 * related to wall time and it doesn't need to start from zero. This
 * should be called before every paint to update the animation.
 *
 * Adjusting the time backwards will give undefined results.
 */
void particle_engine_set_time(struct particle_engine *engine,
			      int32_t msecs);

/**
 * particle_engine_add_color:
 * @engine: A pointer to the particle engine
 * @color: A color to add as 4 bytes representing RGBA
 *
 * Adds a color to the selection of colors that will be initially
 * chosen for a particle. The colors will be selected randomly and
 * distributed evenly for each new particle.
 */
void particle_engine_add_color (struct particle_engine *engine,
				const unsigned char color[4]);

/**
 * particle_engine_remove_color:
 * @engine: A pointer to the particle engine
 * @color: A color to remove as 4 bytes representing RGBA
 *
 * Removes a color from the selection of colors that will be initially
 * chosen for a particle.
 */
void particle_engine_remove_color (struct particle_engine *engine,
				   const unsigned char color[4]);

void particle_engine_paint(struct particle_engine *engine);

#endif /* _PARTICLE_ENGINE_H_ */
