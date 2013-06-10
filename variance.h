#ifndef _VARIANCE_H
#define _VARIANCE_H

#include <glib.h>

/*
 * A double precision variance.
 */
struct double_variance {
	gdouble value;
	gdouble variance;
	enum {
		DOUBLE_VARIANCE_NONE,
		DOUBLE_VARIANCE_LINEAR,
		DOUBLE_VARIANCE_PROPORTIONAL
	} type;
};

gdouble double_variance_get_value(struct double_variance *variance,
				  GRand *rand);

/*
 * A three dimensional vector variance.
 */
struct vector_variance {
	float value[3];
	float variance[3];
	enum {
		VECTOR_VARIANCE_NONE,
		VECTOR_VARIANCE_LINEAR,
		VECTOR_VARIANCE_PROPORTIONAL
	} type;

	float out[3];
};

void vector_variance_get_value(struct vector_variance *variance,
			       GRand *rand, float *value);

#endif /* _VARIANCE_H */
