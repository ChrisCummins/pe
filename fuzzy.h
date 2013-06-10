#ifndef _VARIANCE_H
#define _VARIANCE_H

#include <glib.h>

/*
 * A double precision variance.
 */
struct fuzzy_double {
	gdouble value;
	gdouble variance;
	enum {
		DOUBLE_VARIANCE_NONE,
		DOUBLE_VARIANCE_LINEAR,
		DOUBLE_VARIANCE_PROPORTIONAL
	} type;
};

gdouble fuzzy_double_get_value(struct fuzzy_double *variance,
			       GRand *rand);

/*
 * A three dimensional vector variance.
 */
struct fuzzy_vector {
	float value[3];
	float variance[3];
	enum {
		VECTOR_VARIANCE_NONE,
		VECTOR_VARIANCE_LINEAR,
		VECTOR_VARIANCE_PROPORTIONAL
	} type;

	float out[3];
};

void fuzzy_vector_get_value(struct fuzzy_vector *variance,
			    GRand *rand, float *value);

#endif /* _VARIANCE_H */
