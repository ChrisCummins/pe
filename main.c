#include "config.h"

#include "particle-engine.h"

#include <cogl/cogl.h>

#define WIDTH 1024
#define HEIGHT 768

struct pe {
	CoglContext *ctx;
	CoglFramebuffer *fb;
	CoglMatrix view;
	int width, height;

	struct particle_engine *engine[5];

	guint timeout_id;

	CoglBool swap_ready;
	GMainLoop *main_loop;
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

static gboolean _timeout_cb(gpointer data)
{
	struct pe *pe = data;
	unsigned int i;

	for (i = 0; i < 3; i++)
		pe->engine[i]->source_active = !pe->engine[i]->source_active;

	return TRUE;
}

static gboolean _update_cb(gpointer data)
{
	struct pe *pe = data;
	CoglPollFD *poll_fds;
	int n_poll_fds;
	int64_t timeout;

	if (pe->swap_ready) {
		paint_cb(pe);
		cogl_onscreen_swap_buffers(COGL_ONSCREEN(pe->fb));
	}

	cogl_poll_renderer_get_info(cogl_context_get_renderer(pe->ctx),
				    &poll_fds, &n_poll_fds, &timeout);

	g_poll ((GPollFD *)poll_fds, n_poll_fds,
		timeout == -1 ? -1 : timeout / 1000);

	cogl_poll_renderer_dispatch(cogl_context_get_renderer(pe->ctx),
				    poll_fds, n_poll_fds);

	return TRUE;
}

int main(int argc, char **argv)
{
	GMainLoop *loop;

	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct pe pe;
	float fovy, aspect, z_near, z_2d, z_far;
	unsigned int i;

	pe.ctx = cogl_context_new (NULL, &error);
	if (!pe.ctx || error != NULL)
		g_error("Failed to create Cogl context\n");

	onscreen = cogl_onscreen_new(pe.ctx, WIDTH, HEIGHT);

	pe.fb = COGL_FRAMEBUFFER(onscreen);
	pe.width = cogl_framebuffer_get_width(pe.fb);
	pe.height = cogl_framebuffer_get_height(pe.fb);

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
	pe.swap_ready = TRUE;

	cogl_onscreen_add_frame_callback(COGL_ONSCREEN(pe.fb),
					 frame_event_cb, &pe, NULL);
	pe.timeout_id = g_timeout_add(5000, _timeout_cb, &pe);

	for (i = 0; i < G_N_ELEMENTS(pe.engine); i++) {
		pe.engine[i] = particle_engine_new(pe.ctx, pe.fb);

		pe.engine[i]->max_particles = 10000;
		pe.engine[i]->point_size = 3.0f;

		pe.engine[i]->min_initial_position[1] = (float)HEIGHT + 5;
		pe.engine[i]->max_initial_position[1] = (float)HEIGHT + 5;

		pe.engine[i]->min_initial_velocity[0] = -50.0f;
		pe.engine[i]->min_initial_velocity[1] = -600.0f;
		pe.engine[i]->min_initial_velocity[2] = -150.0f;

		pe.engine[i]->max_initial_velocity[0] = 50.0f;
		pe.engine[i]->max_initial_velocity[1] = -900.0f;
		pe.engine[i]->max_initial_velocity[2] = 150.0f;
	}

	/* fountain 1 */
	pe.engine[0]->source_active = FALSE;

	pe.engine[0]->min_initial_velocity[1] = -800.0f;
	pe.engine[0]->max_initial_velocity[1] = -1000.0f;

	pe.engine[0]->min_initial_position[0] = (float)WIDTH / 2 - 5;
	pe.engine[0]->min_initial_position[2] = 0.0f;

	pe.engine[0]->max_initial_position[0] = (float) WIDTH / 2 + 5;
	pe.engine[0]->max_initial_position[2] = 0.0f;

	/* fountain 2 */
	pe.engine[1]->min_initial_position[0] = (float)WIDTH / 4 - 5;
	pe.engine[1]->min_initial_position[2] = 0.0f;

	pe.engine[1]->max_initial_position[0] = (float) WIDTH / 4 + 5;
	pe.engine[1]->max_initial_position[2] = 0.0f;

	/* fountain 3 */
	pe.engine[2]->min_initial_position[0] = ((float)WIDTH / 4) * 3 - 5;
	pe.engine[2]->min_initial_position[2] = 0.0f;

	pe.engine[2]->max_initial_position[0] = ((float) WIDTH / 4) * 3 + 5;
	pe.engine[2]->max_initial_position[2] = 0.0f;

	/* fountain 4 */
	pe.engine[3]->max_particles = 2000;

	pe.engine[3]->min_initial_position[0] = -5.0f;
	pe.engine[3]->min_initial_position[2] = 0.0f;

	pe.engine[3]->max_initial_position[0] = 5.0f;
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
	pe.engine[4]->min_initial_position[2] = 0.0f;

	pe.engine[4]->max_initial_position[0] = (float)WIDTH + 5;
	pe.engine[4]->max_initial_position[2] = 0.0f;

	pe.engine[4]->min_initial_velocity[0] = -400.0f;
	pe.engine[4]->min_initial_velocity[1] = -500.0f;
	pe.engine[4]->min_initial_velocity[2] = -150.0f;

	pe.engine[4]->max_initial_velocity[0] = -300.0f;
	pe.engine[4]->max_initial_velocity[1] = -400.0f;
	pe.engine[4]->max_initial_velocity[2] = 150.0f;

	g_idle_add(_update_cb, &pe);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
