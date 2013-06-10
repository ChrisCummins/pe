#include "config.h"

#include "particle-engine.h"

#include <cogl/cogl.h>

#define WIDTH 1024
#define HEIGHT 768

struct pe {
	CoglFramebuffer *fb;
	int width, height;

	CoglMatrix view;
	CoglIndices *indices;
	CoglPrimitive *primitive;
	CoglTexture *texture;
	CoglPipeline *cube_pipeline;

	struct particle_engine *engine[5];

	GTimer *timer;
	CoglBool swap_ready;
	GMainLoop *main_loop;
	CoglContext *ctx;
};

static CoglMatrix identity;
static CoglColor white;

static CoglVertexP3T2 vertices[] =
{
	/* Front face */
	{ /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
	{ /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
	{ /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
	{ /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

	/* Back face */
	{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
	{ /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
	{ /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
	{ /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},

	/* Top face */
	{ /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
	{ /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
	{ /* pos = */  1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
	{ /* pos = */  1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},

	/* Bottom face */
	{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
	{ /* pos = */  1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f},
	{ /* pos = */  1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},
	{ /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},

	/* Right face */
	{ /* pos = */ 1.0f, -1.0f, -1.0f, /* tex coords = */ 1.0f, 0.0f},
	{ /* pos = */ 1.0f,  1.0f, -1.0f, /* tex coords = */ 1.0f, 1.0f},
	{ /* pos = */ 1.0f,  1.0f,  1.0f, /* tex coords = */ 0.0f, 1.0f},
	{ /* pos = */ 1.0f, -1.0f,  1.0f, /* tex coords = */ 0.0f, 0.0f},

	/* Left face */
	{ /* pos = */ -1.0f, -1.0f, -1.0f, /* tex coords = */ 0.0f, 0.0f},
	{ /* pos = */ -1.0f, -1.0f,  1.0f, /* tex coords = */ 1.0f, 0.0f},
	{ /* pos = */ -1.0f,  1.0f,  1.0f, /* tex coords = */ 1.0f, 1.0f},
	{ /* pos = */ -1.0f,  1.0f, -1.0f, /* tex coords = */ 0.0f, 1.0f}
};

static void paint_cb (struct pe *pe) {
	unsigned int i;

	cogl_framebuffer_clear4f(pe->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.0f, 0.0f, 0.0f, 1);

	for (i = 0; i < G_N_ELEMENTS(pe->engine); i++)
		particle_engine_paint(pe->engine[i]);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct pe *pe = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		pe->swap_ready = TRUE;
}

int
main(int argc, char **argv)
{
	CoglContext *ctx;
	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct pe pe;
	float fovy, aspect, z_near, z_2d, z_far;
	CoglDepthState depth_state;
	unsigned int i;

	ctx = cogl_context_new (NULL, &error);
	if (!ctx)
		g_error("Failed to create Cogl context\n");

	onscreen = cogl_onscreen_new(ctx, WIDTH, HEIGHT);

	pe.fb = COGL_FRAMEBUFFER(onscreen);
	pe.width = cogl_framebuffer_get_width(pe.fb);
	pe.height = cogl_framebuffer_get_height(pe.fb);
	pe.timer = g_timer_new();

	cogl_onscreen_show(onscreen);
	cogl_framebuffer_set_viewport(pe.fb, 0, 0, pe.width, pe.height);

	fovy = 45;
	aspect = (float)pe.width / (float)pe.height;
	z_near = 0.1;
	z_2d = 1000;
	z_far = 2000;

	cogl_framebuffer_perspective(pe.fb, fovy, aspect, z_near, z_far);
	cogl_matrix_init_identity(&pe.view);
	cogl_matrix_view_2d_in_perspective(&pe.view, fovy, aspect, z_near, z_2d,
					   pe.width, pe.height);
	cogl_framebuffer_set_modelview_matrix(pe.fb, &pe.view);

	/* Convenient constants: */
	cogl_matrix_init_identity(&identity);
	cogl_color_init_from_4ub(&white, 0xff, 0xff, 0xff, 0xff);

	pe.indices = cogl_get_rectangle_indices(ctx, 6);
	pe.primitive = cogl_primitive_new_p3t2(ctx,
					       COGL_VERTICES_MODE_TRIANGLES,
					       G_N_ELEMENTS(vertices),
					       vertices);
	cogl_primitive_set_indices (pe.primitive, pe.indices, 6 * 6);

	pe.texture = cogl_texture_new_from_file (ctx,
						 "assets/img/cube-texture.jpg",
						 COGL_TEXTURE_NO_SLICING,
						 COGL_PIXEL_FORMAT_ANY,
						 &error);

	if (!pe.texture)
		g_error("Failed to load texture");

	pe.cube_pipeline = cogl_pipeline_new(ctx);
	cogl_pipeline_set_layer_texture(pe.cube_pipeline, 0, pe.texture);

	cogl_depth_state_init(&depth_state);
	cogl_depth_state_set_test_enabled(&depth_state, TRUE);
	cogl_pipeline_set_depth_state(pe.cube_pipeline, &depth_state, NULL);
	pe.swap_ready = TRUE;

	cogl_onscreen_add_frame_callback(COGL_ONSCREEN(pe.fb),
					 frame_event_cb, &pe, NULL);

	for (i = 0; i < G_N_ELEMENTS(pe.engine); i++) {
		pe.engine[i] = particle_engine_new(ctx, pe.fb);

		pe.engine[i]->max_particles = 10000;
		pe.engine[i]->point_size = 3.0f;

		pe.engine[i]->min_initial_velocity[0] = -50.0f;
		pe.engine[i]->min_initial_velocity[1] = -600.0f;
		pe.engine[i]->min_initial_velocity[2] = -150.0f;

		pe.engine[i]->max_initial_velocity[0] = 50.0f;
		pe.engine[i]->max_initial_velocity[1] = -900.0f;
		pe.engine[i]->max_initial_velocity[2] = 150.0f;
	}

	/* fountain 1 */
	pe.engine[0]->min_initial_velocity[1] = -800.0f;
	pe.engine[0]->max_initial_velocity[1] = -1000.0f;

	pe.engine[0]->min_initial_position[0] = (float)WIDTH / 2 - 5;
	pe.engine[0]->min_initial_position[1] = (float)HEIGHT;
	pe.engine[0]->min_initial_position[2] = 0.0f;

	pe.engine[0]->max_initial_position[0] = (float) WIDTH / 2 + 5;
	pe.engine[0]->max_initial_position[1] = (float)HEIGHT;
	pe.engine[0]->max_initial_position[2] = 0.0f;

	/* fountain 2 */
	pe.engine[1]->min_initial_position[0] = (float)WIDTH / 4 - 5;
	pe.engine[1]->min_initial_position[1] = (float)HEIGHT;
	pe.engine[1]->min_initial_position[2] = 0.0f;

	pe.engine[1]->max_initial_position[0] = (float) WIDTH / 4 + 5;
	pe.engine[1]->max_initial_position[1] = (float)HEIGHT;
	pe.engine[1]->max_initial_position[2] = 0.0f;

	/* fountain 3 */
	pe.engine[2]->min_initial_position[0] = ((float)WIDTH / 4) * 3 - 5;
	pe.engine[2]->min_initial_position[1] = (float)HEIGHT;
	pe.engine[2]->min_initial_position[2] = 0.0f;

	pe.engine[2]->max_initial_position[0] = ((float) WIDTH / 4) * 3 + 5;
	pe.engine[2]->max_initial_position[1] = (float)HEIGHT;
	pe.engine[2]->max_initial_position[2] = 0.0f;

	/* fountain 4 */
	pe.engine[3]->max_particles = 2000;

	pe.engine[3]->min_initial_position[0] = -5.0f;
	pe.engine[3]->min_initial_position[1] = (float)HEIGHT;
	pe.engine[3]->min_initial_position[2] = 0.0f;

	pe.engine[3]->max_initial_position[0] = 5.0f;
	pe.engine[3]->max_initial_position[1] = (float)HEIGHT;
	pe.engine[3]->max_initial_position[2] = 0.0f;

	pe.engine[3]->min_initial_velocity[0] = 300.0f;
	pe.engine[3]->min_initial_velocity[1] = -500.0f;
	pe.engine[3]->min_initial_velocity[2] = -150.0f;

	pe.engine[3]->max_initial_velocity[0] = 400.0f;
	pe.engine[3]->max_initial_velocity[1] = -400.0f;
	pe.engine[3]->max_initial_velocity[2] = 150.0f;

	/* fountain 5 */
	pe.engine[4]->max_particles = 2000;

	pe.engine[4]->min_initial_position[0] = (float)WIDTH -5;
	pe.engine[4]->min_initial_position[1] = (float)HEIGHT;
	pe.engine[4]->min_initial_position[2] = 0.0f;

	pe.engine[4]->max_initial_position[0] = (float)WIDTH + 5;
	pe.engine[4]->max_initial_position[1] = (float)HEIGHT;
	pe.engine[4]->max_initial_position[2] = 0.0f;

	pe.engine[4]->min_initial_velocity[0] = -400.0f;
	pe.engine[4]->min_initial_velocity[1] = -500.0f;
	pe.engine[4]->min_initial_velocity[2] = -150.0f;

	pe.engine[4]->max_initial_velocity[0] = -300.0f;
	pe.engine[4]->max_initial_velocity[1] = -400.0f;
	pe.engine[4]->max_initial_velocity[2] = 150.0f;

	while (1) {
		CoglPollFD *poll_fds;
		int n_poll_fds;
		int64_t timeout;

		if (pe.swap_ready) {
			paint_cb(&pe);
			cogl_onscreen_swap_buffers(COGL_ONSCREEN(pe.fb));
		}

		cogl_poll_renderer_get_info(cogl_context_get_renderer(ctx),
					    &poll_fds, &n_poll_fds, &timeout);

		g_poll ((GPollFD *)poll_fds, n_poll_fds,
			timeout == -1 ? -1 : timeout / 1000);

		cogl_poll_renderer_dispatch(cogl_context_get_renderer(ctx),
					    poll_fds, n_poll_fds);
	}

	return 0;
}
