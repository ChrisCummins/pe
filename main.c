#include "config.h"

#include "particle-engine.h"

#include <cogl/cogl.h>

#define WIDTH 1024
#define HEIGHT 768

struct demo {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_engine *engine[5];

	guint timeout_id;

	CoglBool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb (struct demo *demo) {
	unsigned int i;

	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.15f, 0.15f, 0.3f, 1);

	for (i = 0; i < G_N_ELEMENTS(demo->engine); i++)
		particle_engine_paint(demo->engine[i]);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct demo *demo = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		demo->swap_ready = TRUE;
}

static gboolean _timeout_cb(gpointer data)
{
	struct demo *demo = data;
	unsigned int i;

	for (i = 0; i < 3; i++)
		demo->engine[i]->source_active = !demo->engine[i]->source_active;

	return TRUE;
}

static gboolean _update_cb(gpointer data)
{
	struct demo *demo = data;
	CoglPollFD *poll_fds;
	int n_poll_fds;
	int64_t timeout;

	if (demo->swap_ready) {
		paint_cb(demo);
		cogl_onscreen_swap_buffers(COGL_ONSCREEN(demo->fb));
	}

	cogl_poll_renderer_get_info(cogl_context_get_renderer(demo->ctx),
				    &poll_fds, &n_poll_fds, &timeout);

	g_poll ((GPollFD *)poll_fds, n_poll_fds,
		timeout == -1 ? -1 : timeout / 1000);

	cogl_poll_renderer_dispatch(cogl_context_get_renderer(demo->ctx),
				    poll_fds, n_poll_fds);

	return TRUE;
}

int main(int argc, char **argv)
{
	GMainLoop *loop;

	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct demo demo;
	float fovy, aspect, z_near, z_2d, z_far;
	unsigned int i;

	demo.ctx = cogl_context_new (NULL, &error);
	if (!demo.ctx || error != NULL)
		g_error("Failed to create Cogl context\n");

	onscreen = cogl_onscreen_new(demo.ctx, WIDTH, HEIGHT);

	demo.fb = COGL_FRAMEBUFFER(onscreen);
	demo.width = cogl_framebuffer_get_width(demo.fb);
	demo.height = cogl_framebuffer_get_height(demo.fb);

	cogl_onscreen_show(onscreen);
	cogl_framebuffer_set_viewport(demo.fb, 0, 0, demo.width, demo.height);

	fovy = 45;
	aspect = (float)demo.width / (float)demo.height;
	z_near = 0.1;
	z_2d = 1000;
	z_far = 2000;

	cogl_framebuffer_perspective(demo.fb, fovy, aspect, z_near, z_far);
	cogl_matrix_init_identity(&demo.view);
	cogl_matrix_view_2d_in_perspective(&demo.view, fovy, aspect, z_near, z_2d,
					   demo.width, demo.height);
	cogl_framebuffer_set_modelview_matrix(demo.fb, &demo.view);
	demo.swap_ready = TRUE;

	cogl_onscreen_add_frame_callback(COGL_ONSCREEN(demo.fb),
					 frame_event_cb, &demo, NULL);
	demo.timeout_id = g_timeout_add(5000, _timeout_cb, &demo);

	for (i = 0; i < G_N_ELEMENTS(demo.engine); i++) {
		demo.engine[i] = particle_engine_new(demo.ctx, demo.fb);

		demo.engine[i]->particle_count = 10000;
		demo.engine[i]->particle_size = 3.0f;

		demo.engine[i]->particle_lifespan = 2.0f;
		demo.engine[i]->particle_lifespan_variance = 0.75f;
		demo.engine[i]->particle_lifespan_variance_type = VARIANCE_LINEAR;

		demo.engine[i]->min_initial_position[1] = (float)HEIGHT + 5;
		demo.engine[i]->max_initial_position[1] = (float)HEIGHT + 5;

		demo.engine[i]->min_initial_velocity[0] = -50.0f;
		demo.engine[i]->min_initial_velocity[1] = -600.0f;
		demo.engine[i]->min_initial_velocity[2] = -150.0f;

		demo.engine[i]->max_initial_velocity[0] = 50.0f;
		demo.engine[i]->max_initial_velocity[1] = -900.0f;
		demo.engine[i]->max_initial_velocity[2] = 150.0f;

		demo.engine[i]->new_particles_per_ms = 4000;
	}

	/* fountain 1 */
	demo.engine[0]->source_active = FALSE;

	demo.engine[0]->min_initial_velocity[1] = -800.0f;
	demo.engine[0]->max_initial_velocity[1] = -1000.0f;

	demo.engine[0]->min_initial_position[0] = (float)WIDTH / 2 - 5;
	demo.engine[0]->min_initial_position[2] = 0.0f;

	demo.engine[0]->max_initial_position[0] = (float) WIDTH / 2 + 5;
	demo.engine[0]->max_initial_position[2] = 0.0f;

	/* fountain 2 */
	demo.engine[1]->min_initial_position[0] = (float)WIDTH / 4 - 5;
	demo.engine[1]->min_initial_position[2] = 0.0f;

	demo.engine[1]->max_initial_position[0] = (float) WIDTH / 4 + 5;
	demo.engine[1]->max_initial_position[2] = 0.0f;

	/* fountain 3 */
	demo.engine[2]->min_initial_position[0] = ((float)WIDTH / 4) * 3 - 5;
	demo.engine[2]->min_initial_position[2] = 0.0f;

	demo.engine[2]->max_initial_position[0] = ((float) WIDTH / 4) * 3 + 5;
	demo.engine[2]->max_initial_position[2] = 0.0f;

	/* fountain 4 */
	demo.engine[3]->particle_count = 2000;

	demo.engine[3]->min_initial_position[0] = -5.0f;
	demo.engine[3]->min_initial_position[2] = 0.0f;

	demo.engine[3]->max_initial_position[0] = 5.0f;
	demo.engine[3]->max_initial_position[2] = 0.0f;

	demo.engine[3]->min_initial_velocity[0] = 300.0f;
	demo.engine[3]->min_initial_velocity[1] = -500.0f;
	demo.engine[3]->min_initial_velocity[2] = -150.0f;

	demo.engine[3]->max_initial_velocity[0] = 400.0f;
	demo.engine[3]->max_initial_velocity[1] = -400.0f;
	demo.engine[3]->max_initial_velocity[2] = 150.0f;

	/* fountain 5 */
	demo.engine[4]->particle_count = 2000;

	demo.engine[4]->min_initial_position[0] = (float)WIDTH -5;
	demo.engine[4]->min_initial_position[2] = 0.0f;

	demo.engine[4]->max_initial_position[0] = (float)WIDTH + 5;
	demo.engine[4]->max_initial_position[2] = 0.0f;

	demo.engine[4]->min_initial_velocity[0] = -400.0f;
	demo.engine[4]->min_initial_velocity[1] = -500.0f;
	demo.engine[4]->min_initial_velocity[2] = -150.0f;

	demo.engine[4]->max_initial_velocity[0] = -300.0f;
	demo.engine[4]->max_initial_velocity[1] = -400.0f;
	demo.engine[4]->max_initial_velocity[2] = 150.0f;

	g_idle_add(_update_cb, &demo);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
