#ifndef _PARTICLE_SWARM_H_
#define _PARTICLE_SWARM_H_

#include "fuzzy.h"

/* <priv> */
struct particle_swarm_priv;

/*
 * A particle swarm
 */
struct particle_swarm {

	/* The number of particles in the swarm. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;

	float width;
	float height;

	/* The rate at which particles are repelled from the boundaries */
	float boundary_repulsion_rate;

	float particle_speed;
	float particle_sight;

	/* The distance at which particles begin to repel each-other */
	float particle_distance;
	/* The rate at which particles are repelled from each-other */
	float particle_repulsion_rate;

	/* Particle color. */
	struct fuzzy_color particle_color;

	/* <priv> */
	struct particle_swarm_priv *priv;
};

struct particle_swarm *particle_swarm_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_swarm_free(struct particle_swarm *swarm);

void particle_swarm_paint(struct particle_swarm *swarm);

#endif /* _PARTICLE_SWARM_H_ */
