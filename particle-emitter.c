#include "particle-emitter.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <string.h>

struct particle {
	/* Whether the particle is active or not. */
	CoglBool active;

	/* Particle velocity */
	float velocity[3];

	/* The maximum age of this particle in seconds. The particle will linearly
	 * fade out until this age */
	gdouble max_age;

	/* Time to live. This value represents the age of the particle. When it
	 * reaches zero, the particle ist destroyed. */
	gdouble ttl;
};

struct particle_emitter_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	struct particle *particles;
	int active_particles_count;

	GRand *rand;

	CoglContext *ctx;
	CoglFramebuffer *fb;
	struct particle_engine *engine;
};

static void create_resources(struct particle_emitter *emitter)
{
	struct particle_emitter_priv *priv = emitter->priv;

	priv->active_particles_count = 0;

	priv->particles = g_new0(struct particle, emitter->particle_count);

	priv->engine = particle_engine_new(priv->ctx, priv->fb,
					   emitter->particle_count,
					   emitter->particle_size);
}

static void create_particle(struct particle_emitter *emitter,
			    int index)
{
	struct particle_emitter_priv *priv = emitter->priv;
	struct particle *particle = &priv->particles[index];
	float *position, initial_speed, mag;
	CoglColor *color;
	unsigned int i;

	position = particle_engine_get_particle_position(priv->engine, index);
	color = particle_engine_get_particle_color(priv->engine, index);

	/* Get position */
	fuzzy_vector_get_real_value(&emitter->particle_position,
				    emitter->priv->rand, position);
	/* Get speed */
	initial_speed = fuzzy_float_get_real_value(&emitter->particle_speed,
						   emitter->priv->rand);

	/* Get direction */
	fuzzy_vector_get_real_value(&emitter->particle_direction,
				    emitter->priv->rand,
				    &particle->velocity[0]);

	/* Get direction unit vector magnitude */
	mag = sqrt((particle->velocity[0] * particle->velocity[0]) +
		   (particle->velocity[1] * particle->velocity[1]) +
		   (particle->velocity[2] * particle->velocity[2]));

	/* Scale velocity from unit vector */
	for (i = 0; i < 3; i++)
		particle->velocity[i] *= initial_speed / mag;

	/* Set initial color */
	fuzzy_color_get_cogl_color(&emitter->particle_color,
				   emitter->priv->rand, color);

	particle->max_age = fuzzy_double_get_real_value(&emitter->particle_lifespan,
							emitter->priv->rand);
	particle->ttl = particle->max_age;
	particle->active = TRUE;
}

static void destroy_particle(struct particle_emitter *emitter,
			     int index)
{
	struct particle_emitter_priv *priv = emitter->priv;
	struct particle *particle = &priv->particles[index];
	float *position = particle_engine_get_particle_position(priv->engine,
								index);
	CoglColor *color = particle_engine_get_particle_color(priv->engine,
							      index);

	particle->active = FALSE;

	/* Zero the particle */
	memset(position, 0, sizeof(float) * 3);
	cogl_color_init_from_4f(color, 0, 0, 0, 0);
}

static void update_particle(struct particle_emitter *emitter,
			    int index,
			    gdouble tick_time)
{
	struct particle_emitter_priv *priv = emitter->priv;
	struct particle *particle = &priv->particles[index];
	float *position = particle_engine_get_particle_position(priv->engine,
								index);
	CoglColor *color = particle_engine_get_particle_color(priv->engine,
							      index);
	float t, r, g, b, a;
	unsigned int i;

	/* Update position, using v = u + at */
	for (i = 0; i < 3; i++) {
		particle->velocity[i] += emitter->acceleration[i] * tick_time;
		position[i] += particle->velocity[i];
	}

	/* Fade color over time */
	t = tick_time / particle->max_age;
	r = cogl_color_get_red(color) - t;
	g = cogl_color_get_green(color) - t;
	b = cogl_color_get_blue(color) - t;
	a = cogl_color_get_alpha(color) - t;

	cogl_color_init_from_4f(color, r, g, b, a);
}

static void tick(struct particle_emitter *emitter)
{
	struct particle_emitter_priv *priv = emitter->priv;
	struct particle_engine *engine = priv->engine;
	int i, updated_particles = 0, destroyed_particles = 0;
	int new_particles = 0, max_new_particles;
	gdouble tick_time;

	/* Create resources as necessary */
	if (!engine)
		create_resources(emitter);

	/* Update the clocks */
	priv->last_update_time = priv->current_time;
	priv->current_time = g_timer_elapsed(priv->timer, NULL);

	tick_time = priv->current_time - priv->last_update_time;

	/* The maximum number of new particles to create for this tick. This can
	 * be zero, for example in the case where the emitter isn't active.
	 */
	max_new_particles = emitter->active ?
		tick_time * emitter->new_particles_per_ms : 0;

	/* We must first map the particle engine's buffer before reading or
	 * writing particle data.
	 */
	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	/* Iterate over every particle and update/destroy/create as
	 * necessary.
	 */
	for (i = 0; i < emitter->particle_count; i++) {
		struct particle *particle = &priv->particles[i];

		/* Break early if there's nothing left to do */
		if (updated_particles >= priv->active_particles_count &&
		    new_particles >= max_new_particles) {
			break;
		}

		if (particle->active) {
			if (particle->ttl > 0) {
				/* Update the particle's position and color */
				update_particle(emitter, i, tick_time);

				/* Age the particle */
				particle->ttl -= tick_time;
			} else {
				/* If a particle has expired, remove it */
				destroy_particle(emitter, i);
				destroyed_particles++;
			}

			updated_particles++;
		} else if (new_particles < max_new_particles) {
			/* Create a particle */
			create_particle(emitter, i);
			new_particles++;
		}
	}

	/* We can safely unmap the changes we have made to the particle buffer
	 * now.
	 */
	particle_engine_pop_buffer(priv->engine);

	/* Update particle count */
	priv->active_particles_count += new_particles - destroyed_particles;
}

struct particle_emitter* particle_emitter_new(CoglContext *ctx,
					      CoglFramebuffer *fb)
{
	struct particle_emitter *emitter = g_slice_new0(struct particle_emitter);
	struct particle_emitter_priv *priv = g_slice_new0(struct particle_emitter_priv);

	emitter->active = TRUE;

	priv->ctx = cogl_object_ref(ctx);
	priv->fb = cogl_object_ref(fb);

	priv->timer = g_timer_new();
	priv->rand = g_rand_new();

	emitter->priv = priv;

	return emitter;
}

void particle_emitter_free(struct particle_emitter *emitter)
{
	struct particle_emitter_priv *priv = emitter->priv;

	cogl_object_unref(priv->ctx);
	cogl_object_unref(priv->fb);

	g_rand_free(priv->rand);
	g_timer_destroy(priv->timer);

	particle_engine_free(priv->engine);

	g_slice_free(struct particle_emitter_priv, priv);
	g_slice_free(struct particle_emitter, emitter);
}

void particle_emitter_paint(struct particle_emitter *emitter)
{
	tick(emitter);
	particle_engine_paint(emitter->priv->engine);
}
