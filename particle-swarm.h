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
	float depth;
	/* The threshold at which particles are repelled from the boundaries. */
	float boundary_threshold;
	/* The rate at which particles are repelled from the boundaries */
	float boundary_repulsion_rate;

	/* The maximum speed at which particles may move */
	float particle_speed;
	float particle_sight;

	/* The rate at which particles are attracted to each-other */
	float particle_cohesion_rate;

	/* The rate of consistency between particle velocities */
	float particle_velocity_consistency;

	/* The distance at which particles begin to repel each-other */
	float particle_distance;
	/* The rate at which particles are repelled from each-other */
	float particle_repulsion_rate;

	float acceleration[3];

	/* Particle color. */
	struct fuzzy_color particle_color;

	/* <priv> */
	struct particle_swarm_priv *priv;
};

struct particle_swarm *particle_swarm_new(CoglContext *ctx, CoglFramebuffer *fb);

void particle_swarm_free(struct particle_swarm *swarm);

void particle_swarm_paint(struct particle_swarm *swarm);

#endif /* _PARTICLE_SWARM_H_ */
