#include "particle-swarm.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <string.h>

struct particle {
	float velocity[3];
	float speed;
};

struct particle_swarm_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	GRand *rand;

	struct particle *particles;

	/* The hard particle boundaries. */
	float boundary[3];

	/* The soft minimum boundary thresholds at which particles are
	 * repelled. */
	float boundary_min[3];

	/* The soft maximum boundary thresholds at which particles are
	 * repelled. */
	float boundary_max[3];

	float velocity_sum[3];
	float position_sum[3];

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
	int face;

	position = particle_engine_get_particle_position(priv->engine, index);
	color = particle_engine_get_particle_color(priv->engine, index);

	/* Particle color. */
	fuzzy_color_get_cogl_color(&swarm->particle_color, priv->rand, color);

	face = g_rand_int_range(priv->rand, 0, 4);

	switch (face) {
	case 0: /* Top face */
		position[0] = g_rand_double_range(priv->rand, 0, swarm->width);
		position[1] = g_rand_double_range(priv->rand, -500, -20);
		break;
	case 1: /* Right face */
		position[0] = g_rand_double_range(priv->rand,
						  swarm->width + 20,
						  swarm->width + 500);
		position[1] = g_rand_double_range(priv->rand, 0,
						  swarm->height);
		break;
	case 2: /* Bottom face */
		position[0] = g_rand_double_range(priv->rand, 0, swarm->width);
		position[1] = g_rand_double_range(priv->rand,
						  swarm->height + 20,
						  swarm->height + 500);
		break;
	case 3: /* Left face */
		position[0] = g_rand_double_range(priv->rand, -200, -20);
		position[1] = g_rand_double_range(priv->rand, 0,
						  swarm->height);
		break;
	}

	position[2] = g_rand_double_range(priv->rand,
					  -swarm->depth / 2,
					  swarm->depth / 2);
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
	priv->boundary[2] = swarm->depth;

	for (i = 0; i < 3; i++) {
		priv->boundary_min[i] = priv->boundary[i] *
			swarm->boundary_threshold;
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
particle_apply_swarm_cohesion(struct particle_swarm *swarm, int index,
			      float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position, c[3], cohesion;
	int i;

	position = particle_engine_get_particle_position(priv->engine, index);
	cohesion = tick_time * swarm->particle_cohesion_rate;

	for (i = 0; i < 3; i++) {
		/* We calculate the sum of all other particle positions: */
		c[i] = priv->position_sum[i] - position[i];

		/* We divide this sum to produce an average boid position, or
		 * 'center of mass' of the swarm: */
		c[i] /= swarm->particle_count - 1;

		/* We move the boid by an amount proportional to the distance
		 * between it's current position and the center of mass: */
		v[i] += (c[i] - position[i]) * cohesion;
	}
}

/*
 * Rule 3: Boids try to keep a small distance away from other objects (including
 *         other boids, and the swarm boundaries).
 */
static void
particle_apply_seperation(struct particle_swarm *swarm, int index,
			  float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	float *position, accel;
	int i, j;

	position = particle_engine_get_particle_position(priv->engine, index);
	accel = swarm->boundary_repulsion_rate * tick_time;

	/* Particle avoidance */
	for (i = 0; i < swarm->particle_count; i++) {
		if (i != index) {
			float *pos, dx, dy, dz, distance;

			pos = particle_engine_get_particle_position(priv->engine, i);

			dx = position[0] - pos[0];
			dy = position[1] - pos[1];
			dz = position[2] - pos[2];

			distance = sqrt(dx * dx + dy * dy + dz * dz);

			if (distance < swarm->particle_distance) {
				for (j = 0; j < 3; j++) {
					/* FIXME: is this correct? */
					v[j] -= (pos[j] - position[j]) *
						swarm->particle_repulsion_rate;
				}
			}
		}
	}

	/* Boundary avoidance */
	for (i = 0; i < 3; i++) {
		if (position[i] < priv->boundary_min[i])
			v[i] += accel;
		else if (position[i] > priv->boundary_max[i])
			v[i] += -accel;
	}
}

/*
 * Rule 3: Boids try to match velocity with near boids.
 */
static void
particle_apply_swarm_alignment(struct particle_swarm *swarm, int index,
			       float tick_time, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	int i;

	for (i = 0; i < 3; i++) {
		float velocity_sum;

		/* We calculate the sum of all other particle velocities: */
		velocity_sum = (priv->velocity_sum[i] - particle->velocity[i]) /
			(swarm->particle_count - 1);

		/* We divide this value to produce an average boid velocity of
		 * the swarm: */
		v[i] += (velocity_sum - particle->velocity[1]) *
			swarm->particle_velocity_consistency;
	}
}

static void
particle_apply_global_forces(struct particle_swarm *swarm, int index,
			     float tick_time, float *v)
{
	int i;

	for (i = 0; i < 3; i++) {
		v[i] += swarm->acceleration[i] * tick_time;
	}
}

static float
particle_enforce_speed_limit(float *v, float max_speed)
{
	float mag;
	int i;

	mag = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if (mag > max_speed) {
		for (i = 0; i < 3; i++) {
			v[i] = (v[i] / mag) * max_speed;
		}
	}

	return mag > max_speed ? max_speed : mag;
}

static void update_particle(struct particle_swarm *swarm,
			    int index, float tick_time)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	float *position, dv[3] = { 0 }; /* Change in velocity */
	unsigned int i;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Apply the rules of particle behaviour */
	particle_apply_swarm_cohesion(swarm, index, tick_time, &dv[0]);
	particle_apply_seperation(swarm, index, tick_time, &dv[0]);
	particle_apply_swarm_alignment(swarm, index, tick_time, &dv[0]);
	particle_apply_global_forces(swarm, index, tick_time, &dv[0]);

	/* Apply the velocity change */
	for (i = 0; i < 3; i++) {
		particle->velocity[i] += dv[i];
	}

	particle->speed = particle_enforce_speed_limit(particle->velocity, swarm->particle_speed);

	/* Update position */
	for (i = 0; i < 3; i++) {
		position[i] += particle->velocity[i];
	}
}

static void tick(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle_engine *engine = priv->engine;
	float tick_time;
	int i, j;

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

	/* Sum the total velocity and position of all the particles: */
	priv->velocity_sum[0] = priv->velocity_sum[1] = priv->velocity_sum[2] = 0;
	priv->position_sum[0] = priv->position_sum[1] = priv->position_sum[2] = 0;

	for (i = 0; i < swarm->particle_count; i++) {
		float *position = particle_engine_get_particle_position(priv->engine, i);

		for (j = 0; j < 3; j++) {
			priv->velocity_sum[j] += priv->particles[i].velocity[j];
			priv->position_sum[j] += position[j];
		}
	}

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
