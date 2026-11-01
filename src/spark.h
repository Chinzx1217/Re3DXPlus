/* The sparks off a car scraping a wall: the arcade's clCarSparkManager (one a car, under its
 * clCarEffectManager), run each arcade frame and drawn additive after the scene. */
#ifndef SPARK_H
#define SPARK_H

#include "car.h"
#include "mathx.h"
#include "render.h"

#define SPARK_CARS (1 + CAR_RIVALS) /* the player's, then the rivals' */

/* none flying (a new race) */
void spark_reset(void);
/* One arcade frame of car k's sparks: new ones while e says it scrapes (e NULL: the car is
 * off the road, its sparks go), then all of them fly on; body is its model matrix (they keep
 * to its position) */
void spark_step(int k, const float *body, const car_effect *e);
/* Draw them all, as the lights (render_begin in effect) */
void spark_draw(const mat4 *proj, const mat4 *view, render_params *rp);
/* The cars' tyre marks (clTireTrackManager, laid in spark_step): car k's tyre radius and tread
 * (display units), and the marks drawn over the road, before the lights */
void spark_tyre(int k, float radius, float tread);
void spark_draw_tracks(const mat4 *proj, const mat4 *view, render_params *rp);
/* The cars' backfire (clBackFireManager, started and run in spark_step): car k's model (its
 * CARDB_PACKS pack, -1 none, and body 0 stock, 1..3 aero A..C) for its exhausts; and the fires
 * drawn, as the sparks */
void spark_backfire_car(int k, int pack, int body);
void spark_draw_backfire(const mat4 *proj, const mat4 *view, render_params *rp);

#endif
