#include <cogl/cogl.h>
#include <string.h>

#include "particle-engine.h"

void particle_to_string(struct particle *particle) {
	g_print("particle.initial_position = [%f, %f, %f]\n",
		particle->initial_position[0],
		particle->initial_position[1],
		particle->initial_position[2]);

	g_print("particle.initial_color    = rgba(%.2f, %.2f, %.2f, %.2f)\n",
		cogl_color_get_red(&particle->initial_color),
		cogl_color_get_green(&particle->initial_color),
		cogl_color_get_blue(&particle->initial_color),
		cogl_color_get_alpha(&particle->initial_color));

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

	g_print("particle[%d].initial_color    = rgba(%.2f, %.2f, %.2f, %.2f)\n", i,
		cogl_color_get_red(&particle.initial_color),
		cogl_color_get_green(&particle.initial_color),
		cogl_color_get_blue(&particle.initial_color),
		cogl_color_get_alpha(&particle.initial_color));

	g_print("vertices[%d].color            = rgba(%.2f, %.2f, %.2f, %.2f)\n", i,
		cogl_color_get_red(&engine->priv.vertices[i].color),
		cogl_color_get_green(&engine->priv.vertices[i].color),
		cogl_color_get_blue(&engine->priv.vertices[i].color),
		cogl_color_get_alpha(&engine->priv.vertices[i].color));

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
					   4, COGL_ATTRIBUTE_TYPE_FLOAT);

	engine->priv.primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->particle_count,
						   attributes,
						   G_N_ELEMENTS(attributes));

	for (j = 0; j < G_N_ELEMENTS(attributes); j++)
		cogl_object_unref(attributes[j]);

	engine->priv.last_update_time = engine->priv.current_time;
}

static void _get_particle_position(struct particle_engine *engine,
				   const struct particle *particle,
				   gdouble t, float *position)
{
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
			       engine->acceleration[i] * half_elapsed_time2);
	}
}

static void _get_particle_color(struct particle_engine *engine,
				const struct particle *particle,
				gdouble t, CoglColor *color)
{
	gdouble remaining_time = 1.0f - t;

	cogl_color_init_from_4f(color,
				cogl_color_get_red(&particle->initial_color) * remaining_time,
				cogl_color_get_green(&particle->initial_color) * remaining_time,
				cogl_color_get_blue(&particle->initial_color) * remaining_time,
				cogl_color_get_alpha(&particle->initial_color) * remaining_time);
}

static void _get_particle_velocity(struct particle_engine *engine,
				   struct particle *particle)
{
	float initial_speed, mag;

	/* Get speed */
	initial_speed = fuzzy_float_get_real_value(&engine->particle_speed,
						   engine->priv.rand);

	/* Get direction */
	fuzzy_vector_get_real_value(&engine->particle_direction,
				    engine->priv.rand,
				    &particle->initial_velocity[0]);

	/* Get direction unit vector magnitude */
	mag = sqrt((particle->initial_velocity[0] * particle->initial_velocity[0]) +
		   (particle->initial_velocity[1] * particle->initial_velocity[1]) +
		   (particle->initial_velocity[2] * particle->initial_velocity[2]));

	/* Scale to direction unit vector */
	particle->initial_velocity[0] *= initial_speed / mag;
	particle->initial_velocity[1] *= initial_speed / mag;
	particle->initial_velocity[2] *= initial_speed / mag;
}

static void _create_particle(struct particle_engine *engine,
			     struct vertex *vertex, int index)
{
	struct particle *particle = &engine->priv.particles[index];

	/* Set initial position */
	fuzzy_vector_get_real_value(&engine->particle_position,
				    engine->priv.rand,
				    &particle->initial_position[0]);

	/* Set initial velocity */
	_get_particle_velocity(engine, &engine->priv.particles[index]);

	/* Set initial color */
	fuzzy_color_get_cogl_color(&engine->particle_color,
				   engine->priv.rand,
				   &particle->initial_color);

	particle->max_age = fuzzy_double_get_real_value(&engine->particle_lifespan,
							engine->priv.rand);
	particle->creation_time = engine->priv.current_time;

	engine->priv.vertices[index].position[0] = particle->initial_position[0];
	engine->priv.vertices[index].position[1] = particle->initial_position[1];
	engine->priv.vertices[index].position[2] = particle->initial_position[2];

	cogl_color_init_from_4f(&engine->priv.vertices[index].color,
				cogl_color_get_red(&particle->initial_color),
				cogl_color_get_green(&particle->initial_color),
				cogl_color_get_blue(&particle->initial_color),
				cogl_color_get_alpha(&particle->initial_color));

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
		g_error(G_STRLOC " failed to map buffer: %s", error->message);
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
