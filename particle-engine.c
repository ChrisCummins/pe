#include "particle-engine.h"

struct particle_engine {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglPipeline *pipeline;
	CoglPrimitive *primitive;
	CoglAttributeBuffer *attribute_buffer;

	struct vertex *vertices;
	struct particle *particles;
	unsigned char *particle_state_flags;
	int dirty_particles_count;

	/* The number of particles in the engine. */
	int particle_count;

	/* The size (in pixels) of particles. Each particle is represented by a
	 * rectangular point of dimensions particle_size × particle_size. */
	float particle_size;

};

struct vertex {
	float position[3];
	CoglColor color;
};

struct particle_engine *particle_engine_new(CoglContext *ctx,
					    CoglFramebuffer *fb,
					    int particle_count,
					    float particle_size)
{
	struct particle_engine *engine;
	CoglAttribute *attributes[2];
	unsigned int i;

	engine = g_slice_new0(struct particle_engine);

	engine->particle_count = particle_count;
	engine->particle_size = particle_size;

	engine->ctx = cogl_object_ref(ctx);
	engine->fb = cogl_object_ref(fb);

	engine->particle_state_flags = g_new0(unsigned char, engine->particle_count);
	engine->dirty_particles_count = 0;

	engine->pipeline = cogl_pipeline_new(engine->ctx);
	engine->particles = g_new0(struct particle, engine->particle_count);
	engine->vertices = g_new0(struct vertex, engine->particle_count);

	engine->attribute_buffer =
		cogl_attribute_buffer_new(engine->ctx,
					  sizeof(struct vertex) *
					  engine->particle_count, engine->vertices);

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
					   4, COGL_ATTRIBUTE_TYPE_FLOAT);

	engine->primitive =
		cogl_primitive_new_with_attributes(COGL_VERTICES_MODE_POINTS,
						   engine->particle_count,
						   attributes,
						   G_N_ELEMENTS(attributes));

	cogl_pipeline_set_point_size(engine->pipeline, engine->particle_size);
	cogl_primitive_set_n_vertices(engine->primitive, engine->particle_count);

	for (i = 0; i < G_N_ELEMENTS(attributes); i++)
		cogl_object_unref(attributes[i]);

	return engine;
}

void particle_engine_free(struct particle_engine *engine)
{
	cogl_object_unref(engine->ctx);
	cogl_object_unref(engine->fb);
	cogl_object_unref(engine->pipeline);
	cogl_object_unref(engine->primitive);
	cogl_object_unref(engine->attribute_buffer);

	g_free(engine->particles);
	g_free(engine->vertices);

	g_free(engine);
}

inline struct particle *particle_engine_get_particle(struct particle_engine *engine,
						     int index)
{
	return &engine->particles[index];
}

inline void particle_engine_set_particle_state(struct particle_engine *engine,
					       int index,
					       unsigned char state_flag)
{
	unsigned char *flag;

	flag = &engine->particle_state_flags[index];

	if (!*flag && state_flag)
		engine->dirty_particles_count++;
	else if (*flag && !state_flag)
		engine->dirty_particles_count--;

	*flag = state_flag;
}

static void update(struct particle_engine *engine) {
	CoglError *error = NULL;
	int i;

	engine->vertices = cogl_buffer_map(COGL_BUFFER(engine->attribute_buffer),
					   COGL_BUFFER_ACCESS_WRITE,
					   COGL_BUFFER_MAP_HINT_DISCARD, &error);

	if (error != NULL) {
		g_error(G_STRLOC " failed to map buffer: %s", error->message);
		return;
	}

	for (i = 0; i < engine->particle_count; i++) {
		struct particle *particle;
		struct vertex *vertex;
		unsigned char dirty_flag;

		/* Break if there's nothing left to do */
		if (!engine->dirty_particles_count)
			break;

		particle  = &engine->particles[i];
		vertex = &engine->vertices[i];
		dirty_flag = engine->particle_state_flags[i];

		if (dirty_flag) {
			if (dirty_flag & PARTICLE_STATE_DIRTY_POSITION) {
				unsigned int i;

				for (i = 0; i < 3; i++) {
					vertex->position[i] = particle->position[i];
				}
			}

			if (dirty_flag & PARTICLE_STATE_DIRTY_COLOR) {
				cogl_color_init_from_4f(&vertex->color,
							cogl_color_get_red(&particle->color),
							cogl_color_get_green(&particle->color),
							cogl_color_get_blue(&particle->color),
							cogl_color_get_alpha(&particle->color));
			}

			engine->particle_state_flags[i] = PARTICLE_STATE_CLEAN;
			engine->dirty_particles_count--;
		}
	}

	cogl_buffer_unmap(COGL_BUFFER(engine->attribute_buffer));
}

void particle_engine_paint(struct particle_engine *engine)
{
	update(engine);

	cogl_framebuffer_draw_primitive(engine->fb,
					engine->pipeline,
					engine->primitive);
}
