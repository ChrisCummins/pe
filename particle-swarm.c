#include "particle-swarm.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <string.h>

struct particle {
	float radius;

	/* The angular velocity in the orbital plane (in radians per
	 * millisecond). */
	float speed;

	gdouble t_offset;
};

struct particle_swarm_priv {
	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;

	GRand *rand;

	struct particle *particles;

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
	float *position, period, theta, radius, y_rad, x_rad;
	CoglColor *color;

	position = particle_engine_get_particle_position(priv->engine, index);
	color = particle_engine_get_particle_color(priv->engine, index);

	/* Get perpendicular angle */
	theta = fuzzy_float_get_real_value(&swarm->inclination, priv->rand);

	/* Get orbital radius */
	radius = fuzzy_float_get_real_value(&swarm->radius, priv->rand);

	/* Horizontal and vertical components of radius */
	y_rad = sinf(theta) * radius;
	x_rad = sqrt(radius * radius - y_rad * y_rad);

	position[2] = y_rad;
	particle->radius = x_rad;

	/* Orbital velocity */
	particle->speed = swarm->u / particle->radius;

	/* In a circular orbit, the orbital period is:
	 *
	 *    T = 2pi * sqrt(r^3 / μ)
	 *
	 * r - radius
	 * μ - standard gravitational parameter
	 */
	period = 2 * M_PI * sqrt(powf(particle->radius, 3) / swarm->u);

	/* Start the orbit at a random point around it's circumference. */
	particle->t_offset = g_rand_double_range(priv->rand, 0, period);

	/* Particle color. */
	fuzzy_color_get_cogl_color(&swarm->particle_color, priv->rand, color);
}

static void create_resources(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	int i;

	priv->engine = particle_engine_new(priv->ctx, priv->fb,
					   swarm->particle_count,
					   swarm->particle_size);

	priv->particles = g_new0(struct particle, swarm->particle_count);

	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	for (i = 0; i < swarm->particle_count; i++)
		create_particle(swarm, i);

	particle_engine_pop_buffer(priv->engine);
}

static void update_particle(struct particle_swarm *swarm,
			    int index)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle *particle = &priv->particles[index];
	float *position, time, theta, c, s;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Get the particle age. */
	time = particle->t_offset + priv->current_time;

	/* Get the angular position. */
	theta = fmod(time * particle->speed, M_PI * 2);

	/* Derive the X and Y components of this position. */
	c = cos(theta);
	s = sin(theta);

	/* Update the new position. */
	position[0] = swarm->cog[0] + particle->radius * c;
	position[1] = swarm->cog[1] + particle->radius * s;
}

static void tick(struct particle_swarm *swarm)
{
	struct particle_swarm_priv *priv = swarm->priv;
	struct particle_engine *engine = priv->engine;
	int i;

	/* Create resources as necessary */
	if (!engine)
		create_resources(swarm);

	/* Update the clocks */
	priv->last_update_time = priv->current_time;
	priv->current_time = g_timer_elapsed(priv->timer, NULL);

	/* Map the particle engine's buffer before reading or writing particle
	 * data.
	 */
	particle_engine_push_buffer(priv->engine,
				    COGL_BUFFER_ACCESS_READ_WRITE, 0);

	/* Iterate over every particle and update them. */
	for (i = 0; i < swarm->particle_count; i++)
		update_particle(swarm, i);

	/* Unmap the modified particle buffer. */
	particle_engine_pop_buffer(priv->engine);
}

void particle_swarm_paint(struct particle_swarm *swarm)
{
	tick(swarm);
	particle_engine_paint(swarm->priv->engine);
}
