#ifndef _PARTICLE_SWARM_H_
#define _PARTICLE_SWARM_H_

#include "fuzzy.h"

/* <priv> */
struct particle_swarm_priv;

/*
 * A particle swarm
 */
struct particle_swarm {

	/* The type of swarm. */
	enum {
	  	SWARM_TYPE_KEPLER_ORBIT,
	} type;

	/* The position of the center of gravity of the swarm. */
	float cog[3];

	/* The standard gravitational parameter of the swarm center. This is the
	 * product of the gravitational constant (G) and the mass (M) of the
	 * body at the center of gravity.
	 *
	 *      μ = GM
	 */
	float u;

	/* The radius of the swarm. */
	struct fuzzy_float radius;

	/* The inclination of particle orbits, as an angle in radians relative
	 * to the equatorial (reference) plane. */
	struct fuzzy_float inclination;

	/* The number of particles in the swarm. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;

	/* Particle color. */
	struct fuzzy_color particle_color;

	/* <priv> */
	struct particle_swarm_priv *priv;
};

struct particle_swarm *particle_swarm_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_swarm_free(struct particle_swarm *swarm);

void particle_swarm_paint(struct particle_swarm *swarm);

#endif /* _PARTICLE_SWARM_H_ */
