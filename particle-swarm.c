#include "particle-swarm.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <string.h>

/* FIXME: This should be a configurable properties */
#define COHESION_RATE 0.03
#define ALIGNMENT_RATE 0.1

struct particle {
	float position[2];
	float velocity[2];
};

struct particle_swarm_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	GRand *rand;

	struct particle *particles;

	/* The hard particle boundaries. */
	float boundary[2];

	/* The soft minimum boundary thresholds at which particles are
	 * repelled. */
	float boundary_min[2];

	/* The soft maximum boundary thresholds at which particles are
	 * repelled. */
	float boundary_max[2];

	CoglContext *ctx;
	CoglFramebuffer *fb;
	struct particle_engine *engine;
};

struct particle_swarm* particle_swarm_new(CoglContext *ctx,
					  CoglFramebuffer *fb)
{
	struct particle_swarm *swarm = g_slice_new0(struct particle_swarm);
	struct particle_swarm_priv *priv = g_slice_new0(struct particle_swarm_priv);

	priv->ctx = cogl_object_ref(ctx);
	priv->fb = cogl_object_ref(fb);

	priv->timer = g_timer_new();
	priv->rand = g_rand_new();

	swarm->priv = priv;

	return swarm;
}

void particle_swarm_free(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;

	cogl_object_unref(priv->ctx);
	cogl_object_unref(priv->fb);

	g_rand_free(priv->rand);
	g_timer_destroy(priv->timer);

	particle_engine_free(priv->engine);

	g_slice_free(struct particle_swarm_priv, priv);
	g_slice_free(struct particle_swarm, swarm);
}

static void create_particle(struct particle_swarm *swarm,
			    int index)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position;
	CoglColor *color;
	CoglBool seed;

	position = particle_engine_get_particle_position(priv->engine, index);
	color = particle_engine_get_particle_color(priv->engine, index);

	/* Particle color. */
	fuzzy_color_get_cogl_color(&swarm->particle_color, priv->rand, color);

	/* Starting position is a random point along top or bottom */
	position[0] = g_rand_int_range(priv->rand, 0, priv->boundary[0]);

	seed = g_rand_int_range(priv->rand, -1, 1);
	position[1] = seed ? -20 : swarm->height + 20;

	position[2] = 0;
}

static void create_resources(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	int i;

	priv->engine = particle_engine_new(priv->ctx, priv->fb,
					   swarm->particle_count,
					   swarm->particle_size);

	priv->particles = g_new0(struct particle, swarm->particle_count);

	priv->boundary[0] = swarm->width;
	priv->boundary[1] = swarm->height;

	for (i = 0; i < 2; i++) {
		priv->boundary_min[i] = priv->boundary[i] / 4;
		priv->boundary_max[i] = priv->boundary[i] - priv->boundary_min[i];
	}

	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	for (i = 0; i < swarm->particle_count; i++)
		create_particle(swarm, i);

	particle_engine_pop_buffer(priv->engine);
}

/*
 * Rule 1: Boids try to fly towards the centre of mass of neighbouring boids.
 */
static void
update_particle_cohesion(struct particle_swarm *swarm, int index,
			 float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position, c[2] = { 0 };
	int i;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* First, we sum up the positions of all of the other boids: */
	for (i = 0; i < swarm->particle_count; i++) {
		if (i != index) {
			float *pos;

			pos = particle_engine_get_particle_position(priv->engine, i);

			c[0] += pos[0];
			c[1] += pos[1];
		}
	}

	/* We divide this value to produce an average boid position, or 'center
	 * of mass': */
	c[0] /= swarm->particle_count - 1;
	c[1] /= swarm->particle_count - 1;

	/* We move the boid by an amount proportional to the distance between
	 * it's current position and the center of mass: */
	v[0] = (c[0] - position[0]) * tick_time * COHESION_RATE;
	v[1] = (c[1] - position[1]) * tick_time * COHESION_RATE;
}

/*
 * Rule 2: Boids try to keep a small distance away from other objects (including
 *         other boids).
 */
static void
update_particle_seperation(struct particle_swarm *swarm, int index,
			   float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position;
	int i;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Start with a zeroed velocity: */
	v[0] = v[1] = 0;

	/* : */
	for (i = 0; i < swarm->particle_count; i++) {
		if (i != index) {
			float *pos, dx, dy, distance;

			pos = particle_engine_get_particle_position(priv->engine, i);

			dx = position[0] - pos[0];
			dy = position[1] - pos[1];

			distance = sqrt(dx * dx + dy * dy);

			if (distance < swarm->particle_distance) {
				/* FIXME: is this correct? */
				v[0] -= (pos[0] - position[0]) * swarm->particle_repulsion_rate;
				v[1] -= (pos[1] - position[1]) * swarm->particle_repulsion_rate;
			}
		}
	}
}

/*
 * Rule 3: Boids try to match velocity with near boids.
 */
static void
update_particle_alignment(struct particle_swarm *swarm, int index,
			  float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	float c[2] = { 0 };
	int i;

	/* First, we sum up the velocities of all of the other boids: */
	for (i = 0; i < swarm->particle_count; i++) {
		if (i != index) {
			float *vel;

			vel = &priv->particles[index].velocity[0];

			c[0] += vel[0];
			c[1] += vel[1];
		}
	}

	/* We divide this value to produce an average boid velocity, or 'center
	 * of mass': */
	c[0] /= swarm->particle_count - 1;
	c[1] /= swarm->particle_count - 1;

	v[0] = (c[0] - particle->velocity[0]) * ALIGNMENT_RATE;
	v[1] = (c[1] - particle->velocity[1]) * ALIGNMENT_RATE;
}

static void
update_particle_boundaries(struct particle_swarm *swarm, int index,
			   float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position, accel;
	int i;

	position = particle_engine_get_particle_position(priv->engine, index);

	v[0] = v[1] = 0;
	accel = swarm->boundary_repulsion_rate * tick_time;

	for (i = 0; i < 2; i++) {
		if (position[i] < priv->boundary_min[i])
			v[i] = accel;
		else if (position[i] > priv->boundary_max[i])
			v[1] = -accel;
	}
}

static void
enforce_speed_limit(float *v, float max_speed)
{
	float mag;
	int i;

	mag = sqrt(v[0] * v[0] + v[1] * v[1]);

	if (mag > max_speed) {
		for (i = 0; i < 2; i++) {
			v[i] = (v[i] / mag) * max_speed;
		}
	}
}

static void update_particle(struct particle_swarm *swarm,
			    int index, float tick_time)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	float *position, seperation[2], cohesion[2], alignment[2], boundary[2];
	unsigned int i;

	position = particle_engine_get_particle_position(priv->engine, index);

	update_particle_cohesion(swarm, index, tick_time, &cohesion[0]);
	update_particle_seperation(swarm, index, tick_time, &seperation[0]);
	update_particle_alignment(swarm, index, tick_time, &alignment[0]);
	update_particle_boundaries(swarm, index, tick_time, &boundary[0]);

	for (i = 0; i < 2; i++) {
		/* Sum individual velocity changes */
		particle->velocity[i] += seperation[i] + cohesion[i] + alignment[i] + boundary[i];

		enforce_speed_limit(particle->velocity, swarm->particle_speed);

		/* Update position */
		position[i] += particle->velocity[i];
	}
}

static void tick(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle_engine *engine = priv->engine;
	float tick_time;
	int i;

	/* Create resources as necessary */
	if (!engine)
		create_resources(swarm);

	/* Update the clocks */
	priv->last_update_time = priv->current_time;
	priv->current_time = g_timer_elapsed(priv->timer, NULL);
	tick_time = priv->current_time - priv->last_update_time;

	/* Map the particle engine's buffer before reading or writing particle
	 * data.
	 */
	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	/* Iterate over every particle and update them. */
	for (i = 0; i < swarm->particle_count; i++)
		update_particle(swarm, i, tick_time);

	/* Unmap the modified particle buffer. */
	particle_engine_pop_buffer(priv->engine);
}

void particle_swarm_paint(struct particle_swarm *swarm)
{
	tick(swarm);
	particle_engine_paint(swarm->priv->engine);
}
