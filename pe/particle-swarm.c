#include "particle-swarm.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <math.h>
#include <string.h>

#define MAX_FRAME_TIME 0.015
#define DT 0.005

struct particle {
	float velocity[3];
	float speed;
	float size;
};

struct particle_swarm_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble accumulator;

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

	/* Total velocity and position vector sums for the swarm, used only in
	 * SWARM_TYPE_HIVE swarms. It is updated once per tick. */
	float velocity_sum[3];
	float position_sum[3];

	/* Strength of cohesion and boundary forces, updated once per tick. */
	float cohesion_accel;
	float boundary_accel;

	/* Global acceleration force vector, updated once per tick. */
	float global_accel[3];

	struct {
		float min;
		float max;
	} speed_limits;

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
	struct particle *particle = &priv->particles[index];
	float *position;
	CoglColor *color;
	int i;

	position = particle_engine_get_particle_position(priv->engine, index);
	color = particle_engine_get_particle_color(priv->engine, index);

	particle->speed = 1;
	particle->size = g_rand_double(priv->rand) + 0.5;

	/* Particle color. */
	cogl_color_init_from_4f(color, 0, 0, 0, 1);

	/* Particles start at a random point within the swarm space */
	for (i = 0; i < 3; i++) {
		position[i] = g_rand_double_range(priv->rand,
						  priv->boundary_min[i],
						  priv->boundary_max[i]);

		/* Random starting velocity */
		particle->velocity[i] = (g_rand_double(priv->rand) - 0.5) * 4;
	}
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

static void particle_apply_swarming_behaviour(struct particle_swarm *swarm,
					      int index, float *v)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	float *position, center_of_mass[3] = {0}, velocity_avg[3] = {0};
	int i, j, swarm_size = 0;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Iterate over every *other* particle */
	for (i = 0; i < swarm->particle_count; i++) {
		if (i != index) {
			float *pos, dx, dy, dz, distance;

			pos = particle_engine_get_particle_position(priv->engine, i);

			dx = position[0] - pos[0];
			dy = position[1] - pos[1];
			dz = position[2] - pos[2];

			/* Get the distance between the other particle and this
			 * particle */
			distance = sqrt(dx * dx + dy * dy + dz * dz);

			/*
			 * COLLISION AVOIDANCE
			 *
			 * Particles try to keep a small distance away from
			 * other particles to prevent them bumping into each
			 * other and reduce the density of the swarm:
			 */
			if (distance < swarm->particle_distance) {
				/* FIXME: is this correct? */
				for (j = 0; j < 3; j++) {
					v[j] -= (pos[j] - position[j]) *
						swarm->particle_repulsion_rate;
				}
			}

			/* If we're using flocking behaviour, then we total up
			 * the velocity and positions of any particles that are
			 * within the range of visibility of the current
			 * particle. */
			if (swarm->type == SWARM_TYPE_FLOCK) {
				if (distance < swarm->particle_sight) {
					struct particle *p = &priv->particles[i];

					for (j = 0; j < 3; j++) {
						center_of_mass[j] += pos[j];
						velocity_avg[j] += p->velocity[j];
					}

					swarm_size++;
				}
			}
		}
	}

	/* Now we iterate through each of the three coordinate axis and apply
	 * the rules of swarming behaviour to each consecutively. */
	for (i = 0; i < 3; i++) {

		switch (swarm->type) {
		case SWARM_TYPE_HIVE:
			/* We calculate the center of mass and average velocity
			 * of the swarm based on the properties of all of the
			 * other particles: */
			center_of_mass[i] = priv->position_sum[i] - position[i];
			velocity_avg[i] = priv->velocity_sum[i] - particle->velocity[i];

			swarm_size = swarm->particle_count - 1;
			break;
		case SWARM_TYPE_FLOCK:
		{
			CoglColor *color;

			color = particle_engine_get_particle_color(priv->engine, index);

			cogl_color_set_red(color, swarm_size ? 1 / swarm_size : 0);

			/* We must always have a flock to compare against, even
			 * if a particle is on it's own: */
			if (swarm_size < 1) {
				for (j = 0; j < 3; j++) {
					center_of_mass[j] = position[j];
				}

				swarm_size = 1;
			}
		}
		break;
		}

		/* Convert the velocity/position totals into weighted
		 * averages: */
		center_of_mass[i] /= swarm_size;
		velocity_avg[i] /= swarm_size;

		/*
		 * PARTICLE COHESION
		 *
		 * Boids try to fly towards the centre of mass of neighbouring
		 * boids. We do this by first calculating a 'center of mass' for
		 * the swarm, and moving the boid by an amount proportional to
		 * it's distance from that center:
		 */
		v[i] += (center_of_mass[i] - position[i]) *
			priv->cohesion_accel;

		/*
		 * SWARM ALIGNMENT
		 *
		 * Boids try to match velocity with other boids nearby, this
		 * creates a pattern of cohesive behaviour, with the swarm
		 * moving in unison:
		 */
		v[i] += (velocity_avg[i] - particle->velocity[1]) *
			swarm->particle_velocity_consistency;

		/*
		 * BOUNDARY AVOIDANCE
		 *
		 * Boids avoid boundaries by being negatively accelerated away
		 * from them when the distance to the boundary is less than a
		 * known threshold:
		 */
		if (position[i] < priv->boundary_min[i])
			v[i] += priv->boundary_accel;
		else if (position[i] > priv->boundary_max[i])
			v[i] -= priv->boundary_accel;
	}
}

/*
 * TERMINAL VELOCITY
 *
 * Particles are rate limited so that their velocity can never exceed a certain
 * amount:
 */
static float particle_enforce_speed_limit(float *v, float max_speed)
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
	particle_apply_swarming_behaviour(swarm, index, &dv[0]);

	for (i = 0; i < 3; i++) {
		/* Apply global force */
		dv[i] += priv->global_accel[i] * tick_time;

		/* Apply the velocity change to the position */
		particle->velocity[i] += dv[i] * particle->speed * swarm->agility;
	}

	/* Limit the rate of particle movement */
	particle->speed = particle_enforce_speed_limit(particle->velocity,
						       priv->speed_limits.max);

	/* Update position */
	for (i = 0; i < 3; i++) {
		position[i] += particle->velocity[i];
	}
}

static void tick(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle_engine *engine = priv->engine;
	int i, j;

	for (i = 0; i < 3; i++) {
		priv->global_accel[i] = swarm->acceleration[i] * DT;
	}

	/* Map the particle engine's buffer before reading or writing particle
	 * data.
	 */
	particle_engine_push_buffer(engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	/* Update the cohesion and boundary forces */
	priv->cohesion_accel = swarm->particle_cohesion_rate * DT;
	priv->boundary_accel = swarm->boundary_repulsion_rate * DT;

	/* Update the speed limits */
	priv->speed_limits.min = swarm->speed_limits.min * DT;
	priv->speed_limits.max = swarm->speed_limits.max * DT;

	if (swarm->type == SWARM_TYPE_HIVE) {
		/* Sum the total velocity and position of all the particles: */
		priv->velocity_sum[0] =
			priv->velocity_sum[1] =
			priv->velocity_sum[2] =
			priv->position_sum[0] =
			priv->position_sum[1] =
			priv->position_sum[2] = 0;

		for (i = 0; i < swarm->particle_count; i++) {
			float *position = particle_engine_get_particle_position(engine, i);

			for (j = 0; j < 3; j++) {
				priv->velocity_sum[j] += priv->particles[i].velocity[j];
				priv->position_sum[j] += position[j];
			}
		}
	}

	/* Iterate over every particle and update them. */
	for (i = 0; i < swarm->particle_count; i++)
		update_particle(swarm, i, DT);

	/* Unmap the modified particle buffer. */
	particle_engine_pop_buffer(engine);
}

void particle_swarm_paint(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle_engine *engine;
	float frame_time, time;

	/* Create resources as necessary */
	if (priv->engine == NULL) {
		create_resources(swarm);
		tick(swarm);
	}

	engine = priv->engine;

	/* Update the clocks */
	time = g_timer_elapsed(priv->timer, NULL);
	frame_time = time - priv->current_time;
	priv->current_time = time;

	/* Enforce a maximum frame time to prevent the "spiral of death" when
	 * operating under heavy load */
	if (frame_time > MAX_FRAME_TIME)
		frame_time = MAX_FRAME_TIME;

	priv->accumulator += frame_time;

	/* Update the simulation state as required */
	for ( ; priv->accumulator >= DT; priv->accumulator -= DT)
		tick(swarm);

	particle_engine_paint(engine);
}
