#include "spark.h"
#include "lights.h"
#include "sparkdb.h"
#include "backfiredb.h"
#include "cardb.h"

#include <math.h>
#include <string.h>
#ifdef WMMT_EGL
#include <stdio.h>
#include <stdlib.h>
#endif

/* clCarSparkManager: 30 clCarSparkPrimitives a car (getMaxSparkPrimitive). A spark is a
 * ribbon of two quads through six points (A..F, relative to where it started from the car):
 * (A, C) the oldest cross-section, (B, D) the one after, (E, F) its head, F size above E.
 * Each frame (update 0x85f534c) the sections move back one (A, C <- B, D <- E, F), the head
 * goes on by the velocity, gravity takes 0.2 off its y (0x92f8258), and its life (13 at
 * birth) runs down; it is drawn while the life is 0 or more. */
#define SPARK_MAX 30
#define SPARK_LIFE 13
#define SPARK_GRAVITY 0.2f

typedef struct {
  float p[6][3]; /* A B C D E F */
  float v[3], rel[3];
  int life;      /* -1: free */
  int tex;       /* 0 car_spark03.tga (image 0x11a), 1 car_spark04.tga (0x11b) */
} spark;

/* clTireTrackManager (one a car, its four clTireTracks): each arcade frame (update 0x85f8368)
 * a wheel lays a cross-section of its mark under it, from its world matrix's translation down
 * the tyre's radius (+0x24), half the tread (+0x20) either side along the wheel's -x: the
 * left end u 1, the right u 0, both v (the distance run so far times 0.208333, 0x93dd0fc, from
 * the third section on) and the effect's strength for the wheel as their alpha. A track keeps
 * the last TIRE_TRACK_VERTEX_NUM (effect_config.lua: 30) ends, the oldest dropped. Drawn
 * (clTireTrack::draw 0x85f770c) as a strip in white with those alphas, sprite/tirepattern.tga,
 * blended over the road. */
#define TRACK_NODES 30
#define TRACK_V 0.208333f
#define TRACK_LIFT 0.05f /* (a hair above the road, off its depth) */
typedef struct { float p[3], u, v, a; } track_node;
typedef struct {
  track_node n[TRACK_NODES]; /* oldest first */
  int count;
  float last[2][3], v;
} track;

/* clBackFireManager (src/backfiredb.h, tools/stage/pack_backfire.py): a fire at each of the
 * car's MF_NULLs; while SInfoCarEffect's flag is on, setup (0x85fe8b8) starts the idle ones
 * with SAMLL_A or MIDDLE_A by int(|velocity|) % 2 (B: SMALL_B, MIDDLE_B), then update (0x85fe694)
 * each frame shows each going one at frame 0..16 at the null, half size, under the car's matrix */
typedef struct { int state, group; } backfire;
#define BACKFIRE_TRIS_MAX 4096

static struct {
  spark s[SPARK_CARS][SPARK_MAX];
  backfire bf[SPARK_CARS][4];
  const backfire_car *bf_car[SPARK_CARS];
  float bf_body[SPARK_CARS][16];
  render_vertex bv[BACKFIRE_TEXTURES][4 * BACKFIRE_TRIS_MAX];
  track t[SPARK_CARS][4];
  float radius[SPARK_CARS], tread[SPARK_CARS];
  render_vertex tv[4 * SPARK_CARS * 4 * (TRACK_NODES / 2)];
  float pos[SPARK_CARS][3]; /* the car's position (its matrix's translation) */
  int rand_at;              /* createSpark's running index into the tables (0x97fda24) */
  int ready;
  render_vertex v[2][4 * 2 * SPARK_CARS * SPARK_MAX];
} S;

void spark_reset(void) {
  S.ready = 1;
  for (int k = 0; k < SPARK_CARS; ++k) {
    for (int i = 0; i < SPARK_MAX; ++i) S.s[k][i].life = -1;
    memset(S.t[k], 0, sizeof S.t[k]);
    for (int i = 0; i < 4; ++i) S.bf[k][i].state = -1;
    if (S.radius[k] <= 0) S.radius[k] = 3.0f, S.tread[k] = 2.0f;
  }
}

void spark_backfire_car(int k, int pack, int body) {
  if (k < 0 || k >= SPARK_CARS) return;
  int n = (int)(sizeof BACKFIRE_CAR / sizeof BACKFIRE_CAR[0]);
  S.bf_car[k] = pack >= 0 && pack < n ? &BACKFIRE_CAR[pack][body & 3] : NULL;
  for (int i = 0; i < 4; ++i) S.bf[k][i].state = -1;
}

static void backfire_step(int k, const float *body, const car_effect *e) {
  const backfire_car *c = S.bf_car[k];
  if (!c) return;
  memcpy(S.bf_body[k], body, sizeof S.bf_body[k]);
  if (e->backfire) {
    int g = ((int)e->speed % 2) + (c->b ? 3 : 0);
    for (int i = 0; i < c->n; ++i)
      if (S.bf[k][i].state == -1) S.bf[k][i].state = 0, S.bf[k][i].group = g;
  }
}

/* after the frame's draw: on to the next frame, done past the last */
static void backfire_advance(int k) {
  for (int i = 0; i < 4; ++i)
    if (S.bf[k][i].state >= 0 && ++S.bf[k][i].state >= BACKFIRE_FRAMES) S.bf[k][i].state = -1;
}

void spark_tyre(int k, float radius, float tread) {
  if (k < 0 || k >= SPARK_CARS) return;
  S.radius[k] = radius > 0 ? radius : 3.0f;
  S.tread[k] = tread > 0 ? tread : 2.0f;
}

static void track_push(track *t, const float p[3], float u, float a) {
  if (t->count == TRACK_NODES) {
    memmove(t->n, t->n + 1, sizeof t->n[0] * (TRACK_NODES - 1));
    t->count--;
  }
  track_node *n = &t->n[t->count++];
  memcpy(n->p, p, sizeof n->p);
  n->u = u, n->v = t->v, n->a = a;
}

/* clTireTrackManager::update for car k's wheel w */
static void track_step(int k, int w, const float *m, float a) {
  track *t = &S.t[k][w];
  float c[3] = { m[12], m[13] - S.radius[k] + TRACK_LIFT, m[14] }, l[3], r[3];
  /* (-tread, 0, 0) through the wheel's matrix, halved */
  for (int j = 0; j < 3; ++j) {
    float h = -S.tread[k] * m[j] * 0.5f;
    l[j] = c[j] - h, r[j] = c[j] + h;
  }
  if (t->count > 2) {
    float d[3];
    for (int j = 0; j < 3; ++j) d[j] = ((l[j] - t->last[0][j]) + (r[j] - t->last[1][j])) * 0.5f;
    t->v += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) * TRACK_V;
  }
  memcpy(t->last[0], l, sizeof l);
  memcpy(t->last[1], r, sizeof r);
  track_push(t, l, 1.0f, a);
  track_push(t, r, 0.0f, a);
}

/* clCarSparkManager::createSpark (0x85f4eb0), clCarEffectManager::update's (strength * 30 *
 * 0.5) of them: each flies off along the effect's direction (unit length, doubled) plus a
 * random vector (doubled again across, y not), from the contact point; the faster, the wider
 * (((int)|v| / 5 + 10) * 0.1) and that number's odd or even picks its picture. A spark is
 * only made when one of the 30 is free. */
static void create(int k, const car_effect *e) {
  int n = (int)(e->spark * (float)SPARK_MAX * 0.5f);
#ifdef WMMT_EGL
  if (getenv("WMMT_SPARK_LOG"))
    fprintf(stderr, "spark car %d: %.3f at %.1f %.1f %.1f dir %.2f %.2f %.2f (car %.1f %.1f %.1f) n %d\n", k, e->spark, e->at[0], e->at[1],
            e->at[2], e->dir[0], e->dir[1], e->dir[2], S.pos[k][0], S.pos[k][1], S.pos[k][2], n);
#endif
  float len = sqrtf(e->dir[0] * e->dir[0] + e->dir[1] * e->dir[1] + e->dir[2] * e->dir[2]);
  float dn[3] = { 0, 0, 0 };
  if (len > 0)
    for (int j = 0; j < 3; ++j) dn[j] = e->dir[j] / len;
  for (int i = 0; i < n; ++i) {
    const float *r = SPARK_RAND[i % 4][S.rand_at];
    S.rand_at = S.rand_at + 1 > 99 ? 0 : S.rand_at + 1;
    float v[3] = { 2.0f * (2.0f * r[0] + dn[0]), 2.0f * r[1] + dn[1], 2.0f * (2.0f * r[2] + dn[2]) };
    spark *s = NULL;
    for (int j = 0; j < SPARK_MAX && !s; ++j)
      if (S.s[k][j].life == -1) s = &S.s[k][j];
    if (!s) continue;
    int w = (int)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) / 5 + 10;
    s->tex = w & 1 ? 0 : 1;
    memset(s->p, 0, sizeof s->p);
    s->p[5][1] = (float)w * 0.1f;
    for (int j = 0; j < 3; ++j) {
      s->rel[j] = e->at[j] - S.pos[k][j];
      s->v[j] = v[j];
    }
    s->life = SPARK_LIFE;
  }
}

void spark_step(int k, const float *body, const car_effect *e) {
  if (k < 0 || k >= SPARK_CARS) return;
  if (!S.ready) spark_reset();
  if (!e) {
    for (int i = 0; i < SPARK_MAX; ++i) S.s[k][i].life = -1;
    memset(S.t[k], 0, sizeof S.t[k]);
    for (int i = 0; i < 4; ++i) S.bf[k][i].state = -1;
    return;
  }
  backfire_advance(k);
  backfire_step(k, body, e);
  for (int w = 0; w < 4; ++w) {
    float a = e->track[w];
    track_step(k, w, e->wheel[w], a < 0 ? 0 : a > 1 ? 1 : a);
  }
  memcpy(S.pos[k], body + 12, sizeof S.pos[k]);
  if (e->spark != 0.0f) create(k, e);
  for (int i = 0; i < SPARK_MAX; ++i) {
    spark *s = &S.s[k][i];
    if (s->life == -1) continue;
    memcpy(s->p[0], s->p[1], sizeof s->p[0]);
    memcpy(s->p[2], s->p[3], sizeof s->p[0]);
    memcpy(s->p[1], s->p[4], sizeof s->p[0]);
    memcpy(s->p[3], s->p[5], sizeof s->p[0]);
    for (int j = 0; j < 3; ++j) {
      s->p[4][j] += s->v[j];
      s->p[5][j] += s->v[j];
    }
    s->v[1] -= SPARK_GRAVITY;
    if (--s->life < 0) s->life = -1;
  }
}

/* clCarSparkPrimitive::draw (0x8622d68): the triangles (A B C) (B C D) (B E D) (E D F), as
 * the quads B D C A and D B E F; u across (A B E 0, C D F 1), v along (A C 0, B D 0.5, E F
 * 1); all in clCarSparkPrimitive's colour (240, 212, 0). clCarSparkManager::draw: depth
 * tested, not written, glBlendFunc(GL_SRC_COLOR, GL_ONE). */
void spark_draw(const mat4 *proj, const mat4 *view, render_params *rp) {
  static const int QUAD[2][4] = { { 1, 3, 2, 0 }, { 3, 1, 4, 5 } };
  static const float U[6] = { 0, 0, 1, 1, 0, 1 }, V[6] = { 0, 0.5f, 0, 0.5f, 1, 1 };
  if (!S.ready) return;
  const float *m = view->m;
  int n[2] = { 0, 0 };
  for (int k = 0; k < SPARK_CARS; ++k)
    for (int i = 0; i < SPARK_MAX; ++i) {
      const spark *s = &S.s[k][i];
      if (s->life == -1) continue;
      float q[6][3];
      for (int c = 0; c < 6; ++c) {
        float p[3];
        for (int j = 0; j < 3; ++j) p[j] = S.pos[k][j] + s->rel[j] + s->p[c][j];
        for (int j = 0; j < 3; ++j) q[c][j] = m[j] * p[0] + m[4 + j] * p[1] + m[8 + j] * p[2] + m[12 + j];
      }
      for (int h = 0; h < 2; ++h) {
        render_vertex *v = S.v[s->tex] + 4 * n[s->tex]++;
        for (int c = 0; c < 4; ++c) {
          int at = QUAD[h][c];
          memcpy(v[c].pos, q[at], sizeof v[c].pos);
          v[c].uv[0] = U[at];
          v[c].uv[1] = V[at];
          v[c].rgba[0] = 240;
          v[c].rgba[1] = 212;
          v[c].rgba[2] = 0;
          v[c].rgba[3] = 255;
        }
      }
    }
  for (int t = 0; t < 2; ++t) {
    uint32_t tex = lights_spark_texture(t);
    if (n[t] && tex) render_sprites_by_colour(proj, S.v[t], n[t], tex, rp);
  }
}

void spark_draw_backfire(const mat4 *proj, const mat4 *view, render_params *rp) {
  if (!S.ready) return;
  const float *m = view->m;
  int n[BACKFIRE_TEXTURES] = { 0 };
  for (int k = 0; k < SPARK_CARS; ++k) {
    const backfire_car *c = S.bf_car[k];
    if (!c) continue;
    const float *b = S.bf_body[k];
    for (int i = 0; i < c->n; ++i) {
      const backfire *f = &S.bf[k][i];
      if (f->state < 0) continue;
      const backfire_frame *fr = &BACKFIRE_FRAME[f->group][f->state];
      for (int t = 0; t < fr->n; ++t) {
        const backfire_tri *tri = &BACKFIRE_TRI[fr->first + t];
        if (tri->tex >= BACKFIRE_TEXTURES || n[tri->tex] >= BACKFIRE_TRIS_MAX) continue;
        render_vertex *v = S.bv[tri->tex] + 4 * n[tri->tex]++;
        for (int q = 0; q < 4; ++q) {
          const float *src = tri->v[q < 3 ? q : 2]; /* (the triangle as a quad, its last corner twice) */
          float l[3], w[3];
          for (int j = 0; j < 3; ++j) l[j] = c->at[i][j] + 0.5f * src[j];
          for (int j = 0; j < 3; ++j) w[j] = b[j] * l[0] + b[4 + j] * l[1] + b[8 + j] * l[2] + b[12 + j];
          for (int j = 0; j < 3; ++j) v[q].pos[j] = m[j] * w[0] + m[4 + j] * w[1] + m[8 + j] * w[2] + m[12 + j];
          v[q].uv[0] = src[3];
          v[q].uv[1] = src[4];
          v[q].rgba[0] = v[q].rgba[1] = v[q].rgba[2] = v[q].rgba[3] = 255;
        }
      }
    }
  }
  for (int t = 0; t < BACKFIRE_TEXTURES; ++t) {
    uint32_t tex = lights_backfire_texture(t);
    if (n[t] && tex) render_sprites(proj, S.bv[t], n[t], tex, rp);
  }
}

void spark_draw_tracks(const mat4 *proj, const mat4 *view, render_params *rp) {
  uint32_t tex = lights_track_texture();
  if (!S.ready || !tex) return;
  const float *m = view->m;
  int q = 0;
  for (int k = 0; k < SPARK_CARS; ++k)
    for (int w = 0; w < 4; ++w) {
      const track *t = &S.t[k][w];
      /* each two sections a quad: (L0 R0 R1 L1), the strip's, skipped while all clear */
      for (int i = 0; i + 3 < t->count; i += 2) {
        const track_node *c[4] = { &t->n[i], &t->n[i + 1], &t->n[i + 3], &t->n[i + 2] };
        if (c[0]->a <= 0 && c[2]->a <= 0) continue;
        render_vertex *v = S.tv + 4 * q++;
        for (int j = 0; j < 4; ++j) {
          const float *p = c[j]->p;
          for (int x = 0; x < 3; ++x) v[j].pos[x] = m[x] * p[0] + m[4 + x] * p[1] + m[8 + x] * p[2] + m[12 + x];
          v[j].uv[0] = c[j]->u;
          v[j].uv[1] = c[j]->v;
          v[j].rgba[0] = v[j].rgba[1] = v[j].rgba[2] = 255;
          v[j].rgba[3] = (uint8_t)(c[j]->a * 255.0f + 0.5f);
        }
      }
    }
  if (q) render_sprites_over(proj, S.tv, q, tex, rp);
}
