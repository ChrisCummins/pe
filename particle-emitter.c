#include <cogl/cogl.h>
#include <string.h>

#include "particle-emitter.h"

#define particle_exists(engine, index) (engine)->priv->active_particles[(index)]

struct particle {
	float position[3];
	float velocity[3];
	CoglColor initial_color;

	/* The maximum age of this particle in seconds. The particle will linearly
	 * fade out until this age */
	gdouble max_age;

	/* Time to live. This value represents the age of the particle. When it
	 * reaches zero, the particle ist destroyed. */
	gdouble ttl;
};

struct vertex {
	float position[3];
	CoglColor color;
};

struct particle_emitter_priv {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglPipeline *pipeline;
	CoglPrimitive *primitive;
	CoglTexture *texture;
	CoglAttributeBuffer *attribute_buffer;

	GRand *rand;

	int active_particles_count;
	CoglBool *active_particles;
	struct particle *particles;
	struct vertex *vertices;

	GTimer *timer;
	gdouble current_time;
	gdouble last_update_time;
};

static CoglPipeline *create_pipeline(struct particle_emitter *engine)
{
	CoglPipeline *pipeline = cogl_pipeline_new(engine->priv->ctx);

	/* Add all of the textures to a separate layer */
	if (engine->priv->texture) {
		cogl_pipeline_set_layer_texture(pipeline, 0, engine->priv->texture);
		cogl_pipeline_set_layer_wrap_mode(pipeline, 0,
						  COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
		cogl_pipeline_set_layer_point_sprite_coords_enabled(pipeline, 0, TRUE, NULL);
	}

	cogl_pipeline_set_point_size(pipeline, engine->particle_size);

	return pipeline;
}

static void create_resources(struct particle_emitter *engine)
{
	CoglAttribute *attributes[2];
	unsigned int i;

	if (engine->priv->pipeline != NULL)
		return;

	engine->priv->pipeline = create_pipeline(engine);
	engine->priv->particles = g_new0(struct particle, engine->particle_count);
	engine->priv->vertices = g_new0(struct vertex, engine->particle_count);

	engine->priv->active_particles_count = 0;
	engine->priv->active_particles = g_new0(CoglBool, engine->particle_count);

	engine->priv->attribute_buffer =
		cogl_attribute_buffer_new(engine->priv->ctx,
					  sizeof(struct vertex) *
					  engine->particle_count, engine->priv->vertices);

	attributes[0] = cogl_attribute_new(engine->priv->attribute_buffer,
					   "cogl_position_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   position),
					   3, COGL_ATTRIBUTE_TYPE_FLOAT);

	attributes[1] = cogl_attribute_new(engine->priv->attribute_buffer,
					   "cogl_color_in",
					   sizeof(struct vertex),
					   G_STRUCT_OFFSET(struct vertex,
							   color),
					   4, COGL_ATTRIBUTE_TYPE_FLOAT);

	engine->priv->primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->particle_count,
						   attributes,
						   G_N_ELEMENTS(attributes));

	for (i = 0; i < G_N_ELEMENTS(attributes); i++)
		cogl_object_unref(attributes[i]);

	engine->priv->last_update_time = engine->priv->current_time;
}

static void create_particle(struct particle_emitter *engine,
			    int index)
{
	struct particle *particle = &engine->priv->particles[index];
	struct vertex *vertex = &engine->priv->vertices[index];
	float initial_speed, mag;
	unsigned int i;

	/* Get position */
	fuzzy_vector_get_real_value(&engine->particle_position,
				    engine->priv->rand,
				    &particle->position[0]);
	/* Get speed */
	initial_speed = fuzzy_float_get_real_value(&engine->particle_speed,
						   engine->priv->rand);

	/* Get direction */
	fuzzy_vector_get_real_value(&engine->particle_direction,
				    engine->priv->rand,
				    &particle->velocity[0]);

	/* Get direction unit vector magnitude */
	mag = sqrt((particle->velocity[0] * particle->velocity[0]) +
		   (particle->velocity[1] * particle->velocity[1]) +
		   (particle->velocity[2] * particle->velocity[2]));

	for (i = 0; i < 3; i++) {
		/* Scale velocity from unit vector */
		particle->velocity[i] *= initial_speed / mag;

		vertex->position[i] = particle->position[i];
	}

	/* Set initial color */
	fuzzy_color_get_cogl_color(&engine->particle_color,
				   engine->priv->rand,
				   &particle->initial_color);

	particle->max_age = fuzzy_double_get_real_value(&engine->particle_lifespan,
							engine->priv->rand);
	particle->ttl = particle->max_age;

	cogl_color_init_from_4f(&engine->priv->vertices[index].color,
				cogl_color_get_red(&particle->initial_color),
				cogl_color_get_green(&particle->initial_color),
				cogl_color_get_blue(&particle->initial_color),
				cogl_color_get_alpha(&particle->initial_color));

	engine->priv->active_particles[index] = TRUE;
}

static void destroy_particle(struct particle_emitter *engine,
			     int index)
{
	struct particle_emitter_priv *priv = engine->priv;

	priv->active_particles[index] = FALSE;

	/* Zero the vertex */
	memset(&priv->vertices[index], 0, sizeof(struct vertex));
}

static void update_particle(struct particle_emitter *engine,
			    int index,
			    gdouble tick_time)
{
	struct particle *particle = &engine->priv->particles[index];
	struct vertex *vertex = &engine->priv->vertices[index];
	gdouble t = particle->ttl / particle->max_age;
	unsigned int i;

	/* Update position, using v = u + at */
	for (i = 0; i < 3; i++) {
		particle->velocity[i] += engine->acceleration[i] * tick_time;
		particle->position[i] += particle->velocity[i];
		vertex->position[i] = particle->position[i];
	}

	/* Update color */
	cogl_color_init_from_4f(&vertex->color,
				cogl_color_get_red(&particle->initial_color) * t,
				cogl_color_get_green(&particle->initial_color) * t,
				cogl_color_get_blue(&particle->initial_color) * t,
				cogl_color_get_alpha(&particle->initial_color) * t);
}

static void tick(struct particle_emitter *engine)
{
	struct particle_emitter_priv *priv = engine->priv;
	int i, updated_particles = 0, destroyed_particles = 0;
	int new_particles = 0, max_new_particles;
	gdouble tick_time;
	CoglError *error = NULL;

	/* Create resources as necessary */
	create_resources(engine);

	/* Update the clocks */
	priv->last_update_time = priv->current_time;
	priv->current_time = g_timer_elapsed(priv->timer, NULL);

	tick_time = priv->current_time - priv->last_update_time;

	/* The maximum number of new particles to create for this tick. This can
	 * be zero, for example in the case where the emitter isn't active.
	 */
	max_new_particles = engine->source_active ?
		tick_time * engine->new_particles_per_ms : 0;

	priv->vertices = cogl_buffer_map(COGL_BUFFER(priv->attribute_buffer),
					 COGL_BUFFER_ACCESS_WRITE,
					 COGL_BUFFER_MAP_HINT_DISCARD, &error);

	if (error != NULL) {
		g_error(G_STRLOC " failed to map buffer: %s", error->message);
		return;
	}

	/* Iterate over every particle and update/destroy/create as
	 * necessary.
	 */
	for (i = 0; i < engine->particle_count; i++) {

		/* Break early if there's nothing left to do */
		if (updated_particles >= priv->active_particles_count &&
		    new_particles >= max_new_particles) {
			break;
		}

		if (particle_exists(engine, i)) {
			struct particle *particle = &priv->particles[i];

			if (particle->ttl > 0) {
				/* Update the particle's position and color */
				update_particle(engine, i, tick_time);

				/* Age the particle */
				particle->ttl -= tick_time;
			} else {
				/* If a particle has expired, remove it */
				destroy_particle(engine, i);
				destroyed_particles++;
			}

			updated_particles++;
		} else if (new_particles < max_new_particles) {
			/* Create a particle */
			create_particle(engine, i);
			new_particles++;
		}
	}

	/* Update particle count */
	priv->active_particles_count += new_particles - destroyed_particles;

	cogl_buffer_unmap(COGL_BUFFER(priv->attribute_buffer));
	cogl_primitive_set_n_vertices(priv->primitive, engine->particle_count);
}

struct particle_emitter* particle_emitter_new(CoglContext *ctx,
					    CoglFramebuffer *fb)
{
	struct particle_emitter *engine = g_slice_new0(struct particle_emitter);
	struct particle_emitter_priv *priv = g_slice_new0(struct particle_emitter_priv);

	engine->source_active = TRUE;

	priv->ctx = cogl_object_ref(ctx);
	priv->fb = cogl_object_ref(fb);

	priv->rand = g_rand_new();
	priv->timer = g_timer_new();

	engine->priv = priv;

	return engine;
}

void particle_emitter_free(struct particle_emitter *engine)
{
	struct particle_emitter_priv *priv = engine->priv;

	cogl_object_unref(priv->ctx);
	cogl_object_unref(priv->fb);
	cogl_object_unref(priv->pipeline);
	cogl_object_unref(priv->primitive);
	cogl_object_unref(priv->texture);
	cogl_object_unref(priv->attribute_buffer);

	g_rand_free(priv->rand);
	g_timer_destroy(priv->timer);

	g_free(priv->active_particles);
	g_free(priv->particles);
	g_free(priv->vertices);

	g_slice_free(struct particle_emitter_priv, priv);
	g_slice_free(struct particle_emitter, engine);
}

void particle_emitter_paint(struct particle_emitter *engine)
{
	tick(engine);

	cogl_framebuffer_draw_primitive(engine->priv->fb,
					engine->priv->pipeline,
					engine->priv->primitive);
}
