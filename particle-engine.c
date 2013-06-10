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
	g_assert(i < engine->max_particles);

	if (!engine->used_particles[i]) {
		g_print("particle[%d] - unused\n\n", i);
		return;
	}

	particle = engine->particles[i];

	g_print("particle[%d].initial_position = [%f, %f, %f]\n", i,
		particle.initial_position[0],
		particle.initial_position[1],
		particle.initial_position[2]);

	g_print("engine->vertices[%d].position = [%f, %f, %f]\n", i,
		engine->vertices[i].position[0],
		engine->vertices[i].position[1],
		engine->vertices[i].position[2]);

	g_print("particle[%d].initial_color    = rgba(%d, %d, %d, %d)\n", i,
		particle.initial_color.r,
		particle.initial_color.g,
		particle.initial_color.b,
		particle.initial_color.a);

	g_print("engine->vertices[%d].color    = rgba(%d, %d, %d, %d)\n", i,
		engine->vertices[i].color.r,
		engine->vertices[i].color.g,
		engine->vertices[i].color.b,
		engine->vertices[i].color.a);

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
	CoglPipeline *pipeline = cogl_pipeline_new(engine->ctx);

	/* Add all of the textures to a separate layer */
	if (engine->texture) {
		cogl_pipeline_set_layer_texture(pipeline, 0, engine->texture);
		cogl_pipeline_set_layer_wrap_mode(pipeline, 0,
						  COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
		cogl_pipeline_set_layer_point_sprite_coords_enabled(pipeline, 0, TRUE, NULL);
	}

	cogl_pipeline_set_point_size(pipeline, engine->point_size);

	return pipeline;
}

static void _create_resources(struct particle_engine *engine)
{
	CoglAttribute *attributes[2];
	int i;
	unsigned int j;

	if (engine->pipeline != NULL)
		return;

	engine->pipeline = _create_pipeline(engine);

	engine->particles = g_new(struct particle, engine->max_particles);
	engine->vertices = g_new0(struct vertex, engine->max_particles);

	engine->used_particles_count = engine->max_particles;
	engine->used_particles = g_new0(CoglBool, engine->used_particles_count);

	/* To begin with none of the particles are used */
	memset(engine->used_particles, 0,
	       engine->used_particles_count * sizeof(CoglBool));

	/* Link together all of the particles in a linked list so we can
	 * quickly find unused particles */
	*(struct particle **) &engine->particles[0] = NULL;

	for (i = 1; i < engine->max_particles; i++) {
		*(struct particle **)&engine->particles[i] =
			engine->particles + i - 1;
	}

	engine->attribute_buffer =
		cogl_attribute_buffer_new(engine->ctx,
					  sizeof(struct vertex) *
					  engine->max_particles, engine->vertices);

	attributes[0] = cogl_attribute_new(engine->attribute_buffer,
					   "cogl_position_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   position),
					   3, COGL_ATTRIBUTE_TYPE_FLOAT);

	attributes[1] = cogl_attribute_new(engine->attribute_buffer,
					   "cogl_color_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   color),
					   4, COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

	engine->primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->max_particles,
						   attributes,
						   G_N_ELEMENTS(attributes));

	for (j = 0; j < G_N_ELEMENTS(attributes); j++)
		cogl_object_unref(attributes[j]);

	engine->last_update_time = engine->current_time;
}

static void _set_initial_position(struct particle_engine *engine,
				  float *position)
{
	int i;

	for (i = 0; i < 3; i++) {
		position[i] = (float)g_rand_double_range(engine->rand,
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
		velocity[i] = (float)g_rand_double_range(engine->rand,
							 engine->min_initial_velocity[i],
							 engine->max_initial_velocity[i]);
	}
}

static void _set_initial_color(struct particle_engine *engine,
			       struct color *color)
{
	unsigned char lum = (unsigned char)g_rand_int_range(engine->rand, 150, 255);

	/* TODO: this should have some parameters for configuring value/randomness */
	color->r = lum;
	color->g = lum;
	color->b = (unsigned char)g_rand_int_range(engine->rand, 230, 255);
	color->a = 255;
}

static double _get_max_age(struct particle_engine *engine)
{
	/* TODO: this should have some parameters for configuring value/randomness */
	return g_rand_double_range(engine->rand, 0.25f, 4.0f);
}

static void _get_particle_position(struct particle_engine *engine,
				   const struct particle *particle,
				   gdouble t, float *position)
{
	/* TODO: this should have some parameters for configuring acceleration
	 * (perhaps a gravity + wind abstraction?) */
	static const float acceleration[3] = { 10.0f, 700.0f, 0.0f };
	float elapsed_time = (float)(engine->current_time - particle->creation_time);
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
	struct particle *particle = &engine->particles[index];

	_set_initial_position(engine, &particle->initial_position[0]);
	_set_initial_velocity(engine, &particle->initial_velocity[0]);
	_set_initial_color(engine, &particle->initial_color);
	particle->max_age = _get_max_age(engine);
	particle->creation_time = engine->current_time;

	engine->vertices[index].position[0] = particle->initial_position[0];
	engine->vertices[index].position[1] = particle->initial_position[1];
	engine->vertices[index].position[2] = particle->initial_position[2];

	engine->vertices[index].color.r = particle->initial_color.r;
	engine->vertices[index].color.g = particle->initial_color.g;
	engine->vertices[index].color.b = particle->initial_color.b;
	engine->vertices[index].color.a = particle->initial_color.a;

	engine->used_particles_count++;
	engine->used_particles[index] = TRUE;
}

static void _update_particle(struct particle_engine *engine,
			     struct vertex *vertex, int index,
			     gdouble particle_age)
{
	struct particle particle = engine->particles[index];

	gdouble t = particle_age / (float)particle.max_age;

	_get_particle_position(engine, &particle, t,
			       &vertex->position[0]);
	_get_particle_color(engine, &particle, t, &vertex->color);
}

static void _destroy_particle(struct particle_engine *engine,
			      struct vertex *vertex, int index)
{
	engine->used_particles_count--;
	engine->used_particles[index] = FALSE;

	/* Zero the vertex */
	memset(vertex, 0, sizeof(struct vertex));
}

static void _particle_engine_update(struct particle_engine *engine)
{
	int i, new_particles = 0, max_new_particles;
	struct vertex *vertices;
	CoglError *error = NULL;

	/* Update the clocks */
	engine->last_update_time = engine->current_time;
	engine->current_time = g_timer_elapsed(engine->timer, NULL);

	/* Calculate how many new particles can be created */
	max_new_particles = (engine->current_time - engine->last_update_time) *
		engine->new_particles_per_ms;

	/* Create resources as necessary */
	_create_resources(engine);

	vertices = cogl_buffer_map(COGL_BUFFER(engine->attribute_buffer),
				   COGL_BUFFER_ACCESS_WRITE,
				   COGL_BUFFER_MAP_HINT_DISCARD, &error);

	if (error != NULL) {
		g_error("failed to map buffer: %s", error->message);
		return;
	}

	for (i = 0; i < engine->max_particles; i++) {
		if (engine->used_particles[i]) {
			gdouble particle_age = engine->current_time -
				engine->particles[i].creation_time;

			if (particle_age >= engine->particles[i].max_age) {
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

	cogl_buffer_unmap(COGL_BUFFER(engine->attribute_buffer));
	cogl_primitive_set_n_vertices(engine->primitive, engine->max_particles);
}

struct particle_engine* particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb)
{
	struct particle_engine *engine = g_slice_new0(struct particle_engine);

	engine->source_active = TRUE;

	engine->rand = g_rand_new();
	engine->timer = g_timer_new();

	engine->ctx = ctx;
	engine->fb = fb;

	return engine;
}

void particle_engine_free(struct particle_engine *engine)
{
	g_rand_free(engine->rand);
	g_slice_free(struct particle_engine, engine);
}

void particle_engine_paint(struct particle_engine *engine)
{
	_particle_engine_update(engine);

	cogl_framebuffer_draw_primitive(engine->fb,
					engine->pipeline,
					engine->primitive);
}
