#include <cogl/cogl.h>
#include <string.h>

#include "particle-engine.h"

void particle_to_string(struct particle *particle) {
	g_print("particle.initial_position = [%f, %f, %f]\n",
		particle->initial_position[0],
		particle->initial_position[1],
		particle->initial_position[2]);

	g_print("particle.initial_color    = rgba(%d, %d, %d, %d)\n",
		particle->initial_color.r,
		particle->initial_color.g,
		particle->initial_color.b,
		particle->initial_color.a);

	g_print("particle.initial_velocity = [%f, %f, %f]\n",
		particle->initial_velocity[0],
		particle->initial_velocity[1],
		particle->initial_velocity[2]);

	g_print("particle.creation_time    = %f\n",
		particle->creation_time);

	g_print("particle.max_age          = %f\n",
		particle->max_age);

	g_print("\n");
}

void color_to_string(struct color *color)
{
	g_print("rgba(%d, %d, %d, %d)\n",
		color->r, color->g, color->b, color->a);
}

void particle_index_to_string(struct particle_engine *engine, int i)
{
	struct particle particle;

	g_assert(i >= 0);
	g_assert(i < engine->particle_count);

	if (!engine->priv.used_particles[i]) {
		g_print("particle[%d] - unused\n\n", i);
		return;
	}

	particle = engine->priv.particles[i];

	g_print("particle[%d].initial_position = [%f, %f, %f]\n", i,
		particle.initial_position[0],
		particle.initial_position[1],
		particle.initial_position[2]);

	g_print("vertices[%d].position         = [%f, %f, %f]\n", i,
		engine->priv.vertices[i].position[0],
		engine->priv.vertices[i].position[1],
		engine->priv.vertices[i].position[2]);

	g_print("particle[%d].initial_color    = rgba(%d, %d, %d, %d)\n", i,
		particle.initial_color.r,
		particle.initial_color.g,
		particle.initial_color.b,
		particle.initial_color.a);

	g_print("vertices[%d].color            = rgba(%d, %d, %d, %d)\n", i,
		engine->priv.vertices[i].color.r,
		engine->priv.vertices[i].color.g,
		engine->priv.vertices[i].color.b,
		engine->priv.vertices[i].color.a);

	g_print("particle[%d].initial_velocity = [%f, %f, %f]\n", i,
		particle.initial_velocity[0],
		particle.initial_velocity[1],
		particle.initial_velocity[2]);

	g_print("particle[%d].creation_time    = %f\n", i,
		particle.creation_time);

	g_print("particle[%d].max_age          = %f\n", i,
		particle.max_age);

	g_print("\n");
}

static CoglPipeline *_create_pipeline(struct particle_engine *engine)
{
	CoglPipeline *pipeline = cogl_pipeline_new(engine->priv.ctx);

	/* Add all of the textures to a separate layer */
	if (engine->priv.texture) {
		cogl_pipeline_set_layer_texture(pipeline, 0, engine->priv.texture);
		cogl_pipeline_set_layer_wrap_mode(pipeline, 0,
						  COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
		cogl_pipeline_set_layer_point_sprite_coords_enabled(pipeline, 0, TRUE, NULL);
	}

	cogl_pipeline_set_point_size(pipeline, engine->particle_size);

	return pipeline;
}

static void _create_resources(struct particle_engine *engine)
{
	CoglAttribute *attributes[2];
	int i;
	unsigned int j;

	if (engine->priv.pipeline != NULL)
		return;

	engine->priv.pipeline = _create_pipeline(engine);

	engine->priv.particles = g_new0(struct particle, engine->particle_count);
	engine->priv.vertices = g_new0(struct vertex, engine->particle_count);

	engine->priv.used_particles_count = engine->particle_count;
	engine->priv.used_particles = g_new0(CoglBool, engine->particle_count);

	for (i = 1; i < engine->particle_count; i++) {
		*(struct particle **)&engine->priv.particles[i] =
			engine->priv.particles + i - 1;
	}

	engine->priv.attribute_buffer =
		cogl_attribute_buffer_new(engine->priv.ctx,
					  sizeof(struct vertex) *
					  engine->particle_count, engine->priv.vertices);

	attributes[0] = cogl_attribute_new(engine->priv.attribute_buffer,
					   "cogl_position_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   position),
					   3, COGL_ATTRIBUTE_TYPE_FLOAT);

	attributes[1] = cogl_attribute_new(engine->priv.attribute_buffer,
					   "cogl_color_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   color),
					   4, COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

	engine->priv.primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->particle_count,
						   attributes,
						   G_N_ELEMENTS(attributes));

	for (j = 0; j < G_N_ELEMENTS(attributes); j++)
		cogl_object_unref(attributes[j]);

	engine->priv.last_update_time = engine->priv.current_time;
}

static void _set_initial_position(struct particle_engine *engine,
				  float *position)
{
	int i;

	for (i = 0; i < 3; i++) {
		position[i] = (float)g_rand_double_range(engine->priv.rand,
							 engine->min_initial_position[i],
							 engine->max_initial_position[i]);
	}
}

static void _set_initial_velocity(struct particle_engine *engine,
				  float *velocity)
{
	int i;

	/* TODO: the random variance in the velocity should be proportional in
	 * each axis. E.g., a large variance in the Z axis should imply a
	 * reduced variance in the X and Y axis. */
	for (i = 0; i < 3; i++) {
		velocity[i] = (float)g_rand_double_range(engine->priv.rand,
							 engine->min_initial_velocity[i],
							 engine->max_initial_velocity[i]);
	}
}

static void _set_initial_color(struct particle_engine *engine,
			       struct color *color)
{
	unsigned char lum = (unsigned char)g_rand_int_range(engine->priv.rand, 150, 255);

	/* TODO: this should have some parameters for configuring value/randomness */
	color->r = lum;
	color->g = lum;
	color->b = (unsigned char)g_rand_int_range(engine->priv.rand, 230, 255);
	color->a = 255;
}

static double _get_max_age(struct particle_engine *engine)
{
	switch (engine->particle_lifespan_variance_type) {

	case VARIANCE_LINEAR:
	{
		gdouble v = engine->particle_lifespan / engine->particle_lifespan_variance;
		return g_rand_double_range(engine->priv.rand,
					   engine->particle_lifespan - v,
					   engine->particle_lifespan + v);
	}
	case VARIANCE_NONE:
	default:
		return engine->particle_lifespan;
	}
}

static void _get_particle_position(struct particle_engine *engine,
				   const struct particle *particle,
				   gdouble t, float *position)
{
	/* TODO: this should have some parameters for configuring acceleration
	 * (perhaps a gravity + wind abstraction?) */
	static const float acceleration[3] = { 10.0f, 700.0f, 0.0f };
	float elapsed_time = (float)(engine->priv.current_time - particle->creation_time);
	float half_elapsed_time2 = (float)(elapsed_time * elapsed_time * 0.5f);
	unsigned int i;

	for (i = 0; i < 3; i++) {
		/* u = initial velocity,
		 * a = acceleration,
		 * t = time
		 *
		 * Displacement:
		 *         s = ut + 0.5×(at²)
		 */
		position[i] = (particle->initial_position[i] +
			       particle->initial_velocity[i] * elapsed_time +
			       acceleration[i] * half_elapsed_time2);
	}
}

static void _get_particle_color(struct particle_engine *engine,
				const struct particle *particle,
				gdouble t, struct color *color)
{
	gdouble remaining_time = 1.0f - t;

	color->r = (unsigned char)(particle->initial_color.r * remaining_time);
	color->g = (unsigned char)(particle->initial_color.g * remaining_time);
	color->b = (unsigned char)(particle->initial_color.b * remaining_time);
	color->a = (unsigned char)(particle->initial_color.a * remaining_time);
}

static void _create_particle(struct particle_engine *engine,
			     struct vertex *vertex, int index)
{
	struct particle *particle = &engine->priv.particles[index];

	_set_initial_position(engine, &particle->initial_position[0]);
	_set_initial_velocity(engine, &particle->initial_velocity[0]);
	_set_initial_color(engine, &particle->initial_color);
	particle->max_age = _get_max_age(engine);
	particle->creation_time = engine->priv.current_time;

	engine->priv.vertices[index].position[0] = particle->initial_position[0];
	engine->priv.vertices[index].position[1] = particle->initial_position[1];
	engine->priv.vertices[index].position[2] = particle->initial_position[2];

	engine->priv.vertices[index].color.r = particle->initial_color.r;
	engine->priv.vertices[index].color.g = particle->initial_color.g;
	engine->priv.vertices[index].color.b = particle->initial_color.b;
	engine->priv.vertices[index].color.a = particle->initial_color.a;

	engine->priv.used_particles_count++;
	engine->priv.used_particles[index] = TRUE;
}

static void _update_particle(struct particle_engine *engine,
			     struct vertex *vertex, int index,
			     gdouble particle_age)
{
	struct particle particle = engine->priv.particles[index];

	gdouble t = particle_age / (float)particle.max_age;

	_get_particle_position(engine, &particle, t,
			       &vertex->position[0]);
	_get_particle_color(engine, &particle, t, &vertex->color);
}

static void _destroy_particle(struct particle_engine *engine,
			      struct vertex *vertex, int index)
{
	engine->priv.used_particles_count--;
	engine->priv.used_particles[index] = FALSE;

	/* Zero the vertex */
	memset(vertex, 0, sizeof(struct vertex));
}

static void _particle_engine_update(struct particle_engine *engine)
{
	int i, new_particles = 0, max_new_particles;
	struct vertex *vertices;
	CoglError *error = NULL;

	/* Update the clocks */
	engine->priv.last_update_time = engine->priv.current_time;
	engine->priv.current_time = g_timer_elapsed(engine->priv.timer, NULL);

	/* Calculate how many new particles can be created */
	max_new_particles = (engine->priv.current_time - engine->priv.last_update_time) *
		engine->new_particles_per_ms;

	/* Create resources as necessary */
	_create_resources(engine);

	vertices = cogl_buffer_map(COGL_BUFFER(engine->priv.attribute_buffer),
				   COGL_BUFFER_ACCESS_WRITE,
				   COGL_BUFFER_MAP_HINT_DISCARD, &error);

	if (error != NULL) {
		g_error("failed to map buffer: %s", error->message);
		return;
	}

	for (i = 0; i < engine->particle_count; i++) {
		if (engine->priv.used_particles[i]) {
			gdouble particle_age = engine->priv.current_time -
				engine->priv.particles[i].creation_time;

			if (particle_age >= engine->priv.particles[i].max_age) {
				/* If a particle has expired, remove it */
				_destroy_particle(engine, &vertices[i], i);
			} else {
				/* Otherwise, update it's position and color */
				_update_particle(engine, &vertices[i], i,
						 particle_age);
			}
		} else if (engine->source_active &&
			   new_particles < max_new_particles) {
			/* Create a particle */
			_create_particle(engine, &vertices[i], i);
			new_particles++;
		}
	}

	cogl_buffer_unmap(COGL_BUFFER(engine->priv.attribute_buffer));
	cogl_primitive_set_n_vertices(engine->priv.primitive, engine->particle_count);
}

struct particle_engine* particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb)
{
	struct particle_engine *engine = g_slice_new0(struct particle_engine);

	engine->source_active = TRUE;

	engine->priv.rand = g_rand_new();
	engine->priv.timer = g_timer_new();

	engine->priv.ctx = ctx;
	engine->priv.fb = fb;

	return engine;
}

void particle_engine_free(struct particle_engine *engine)
{
	g_rand_free(engine->priv.rand);
	g_slice_free(struct particle_engine, engine);
}

void particle_engine_paint(struct particle_engine *engine)
{
	_particle_engine_update(engine);

	cogl_framebuffer_draw_primitive(engine->priv.fb,
					engine->priv.pipeline,
					engine->priv.primitive);
}
