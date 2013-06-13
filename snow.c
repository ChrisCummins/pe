/*
 *         snow.c -- A snowy night scene.
 *
 * This simple demo consists of a single particle engine which emits a steady
 * stream of snowflakes into a light breeze. This demonstrates the support for
 * changing the global acceleration force and particle creation rate in real
 * time.
 */
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

	struct particle_engine *engine;

	GTimer *timer;

	gdouble snow_rate;

	CoglBool swap_ready;
	GMainLoop *main_loop;
};

static void paint_cb(struct demo *demo) {
	cogl_framebuffer_clear4f(demo->fb,
				 COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
				 0.0f, 0.0f, 0.1f, 1);

	particle_engine_paint(demo->engine);
}

static void frame_event_cb(CoglOnscreen *onscreen, CoglFrameEvent event,
			   CoglFrameInfo *info, void *data) {
	struct demo *demo = data;

	if (event == COGL_FRAME_EVENT_SYNC)
		demo->swap_ready = TRUE;
}

static gboolean update_cb(gpointer data)
{
	struct demo *demo = data;
	CoglPollFD *poll_fds;
	int n_poll_fds;
	int64_t timeout;

	/* Change the direction and velocity of wind over time */
	demo->engine->acceleration[0] = 15 *
		sin(0.25 * g_timer_elapsed(demo->timer, NULL));

	/* Change the rate at which new snow appears over time */
	demo->snow_rate += g_random_double_range(0, 0.005);
	demo->engine->new_particles_per_ms = 30 +
		fabs(300 * sin(demo->snow_rate));

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

static void init_particle_engine(struct demo *demo)
{
	demo->engine = particle_engine_new(demo->ctx, demo->fb);
	demo->engine->particle_count = 2000;
	demo->engine->particle_size = 4.0f;
	demo->engine->new_particles_per_ms = 250;

	/* Global force */
	demo->engine->acceleration[0] = 0.0f;
	demo->engine->acceleration[1] = 40.0f;
	demo->engine->acceleration[2] = 0.0f;

	/* Particle position */
	demo->engine->particle_position.value[0] = WIDTH / 2;
	demo->engine->particle_position.variance[0] = WIDTH + WIDTH / 2;
	demo->engine->particle_position.value[1] = -80;
	demo->engine->particle_position.type = VECTOR_VARIANCE_LINEAR;

	/* Particle speed */
	demo->engine->particle_speed.value = 30.0f;
	demo->engine->particle_speed.variance = 1.0f;
	demo->engine->particle_speed.type = FLOAT_VARIANCE_PROPORTIONAL;

	/* Direction */
	demo->engine->particle_direction.value[1] = 0.5f;
	demo->engine->particle_direction.variance[0] = 0.8;
	demo->engine->particle_direction.type = VECTOR_VARIANCE_IRWIN_HALL;

	/* Lifespan */
	demo->engine->particle_lifespan.value = 6.5f;
	demo->engine->particle_lifespan.variance = 1.5f;
	demo->engine->particle_lifespan.type = DOUBLE_VARIANCE_LINEAR;

	/* Color */
	demo->engine->particle_color.saturation.value = 1.0f;
	demo->engine->particle_color.luminance.value = 1.0f;
}

int main(int argc, char **argv)
{
	GMainLoop *loop;
	CoglOnscreen *onscreen;
	CoglError *error = NULL;
	struct demo demo;
	float fovy, aspect, z_near, z_2d, z_far;

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

	init_particle_engine(&demo);

	demo.timer = g_timer_new();
	demo.snow_rate = 0;

	g_idle_add(update_cb, &demo);
	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}