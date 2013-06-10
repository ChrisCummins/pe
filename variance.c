#include "variance.h"

gdouble double_variance_get_value(struct double_variance *variance,
				  GRand *rand)
{
	switch (variance->type) {

	case DOUBLE_VARIANCE_LINEAR:
	{
		gdouble v = variance->variance / 2;
		return g_rand_double_range(rand,
					   variance->value - v,
					   variance->value + v);
	}
	case DOUBLE_VARIANCE_PROPORTIONAL:
	{
		gdouble v = variance->value / variance->variance;
		return g_rand_double_range(rand,
					   variance->value - v,
					   variance->value + v);
	}
	case DOUBLE_VARIANCE_NONE:
	default:
		return variance->value;
	}
}

void vector_variance_get_value(struct vector_variance *variance,
			       GRand *rand,
			       float *value)
{
	unsigned int i;

	switch (variance->type) {

	case VECTOR_VARIANCE_LINEAR:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++) {
			float v = variance->variance[i] / 2;

			value[i] = (float)g_rand_double_range(rand,
							      variance->value[i] - v,
							      variance->value[i] + v);
		}
		break;
	case VECTOR_VARIANCE_PROPORTIONAL:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++) {
			float v = variance->value[i] / variance->variance[i];

			value[i] = (float)g_rand_double_range(rand,
							      variance->value[i] - v,
							      variance->value[i] + v);
		}
		break;
	case VECTOR_VARIANCE_NONE:
	default:
		for (i = 0; i < G_N_ELEMENTS(variance->value); i++)
			value[i] = variance->value[i];
		break;
	}
}
