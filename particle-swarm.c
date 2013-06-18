#include "particle-swarm.h"

#include "particle-engine.h"

#include <cogl/cogl.h>
#include <string.h>

struct particle {
	/* The radius of the orbit */
	float radius;

	/* The angular velocity in the orbital plane (in radians per
	 * millisecond). */
	float speed;

	/* The orbital period offset, in seconds. */
	gdouble t_offset;

	/* Longitude of ascending node, in radians. */
	float ascending_node;

	/* Inclination in radians from equatorial plane. If inclination is >
	 * pi/2, orbit is retrograde. */
	float inclination;
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
	float period;
	CoglColor *color;

	color = particle_engine_get_particle_color(priv->engine, index);

	/* Get angle of inclination */
	particle->inclination = fuzzy_float_get_real_value(&swarm->inclination, priv->rand);

	/* Get the ascending node */
	particle->ascending_node = g_random_double_range(0, M_PI * 2);

	/* Get orbital radius */
	particle->radius = fuzzy_float_get_real_value(&swarm->radius, priv->rand);

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
	float *position, time, theta, x, y, z;

	position = particle_engine_get_particle_position(priv->engine, index);

	/* Get the particle age. */
	time = particle->t_offset + priv->current_time;

	/* Get the angular position. */
	theta = fmod(time * particle->speed, M_PI * 2);

	/* Object space coordinates. */
	x = cosf(theta) * particle->radius;
	y = sinf(theta) * particle->radius;
	z = 0;

	/* FIXME: Rotate around Z axis to the ascending node */
	/* x = x * cosf(particle->ascending_node) - y * sinf(particle->ascending_node); */
	/* y = x * sinf(particle->ascending_node) + y * cosf(particle->ascending_node); */

	/* FIXME: Rotate about X axis to the inclination */
	/* y = y * cosf(particle->inclination) - z * sinf(particle->inclination); */
	/* z = y * sinf(particle->inclination) + z * cosf(particle->inclination); */

	/* Update the new position. */
	position[0] = swarm->cog[0] + x;
	position[1] = swarm->cog[1] + y;
	position[2] = swarm->cog[2] + z;
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
