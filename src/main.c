#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#elif defined(__APPLE__)
#define SOKOL_METAL
#else
#define SOKOL_GLCORE
#endif
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gl.h"
#include "sokol_debugtext.h"
#include "sokol_log.h"

#define CECS_MAX_ENTITIES 640u
#define CECS_MAX_COMPONENTS 16u
#define CECS_MAX_SYSTEMS 8u
#define CECS_COMPONENT_STORAGE_BYTES 65536u
#define CECS_IMPLEMENTATION
#include "cecs.h"

#include "generated_assets.h"

#define PI 3.14159265358979323846f
#define WORLD_W 18.0f
#define WORLD_H 30.0f
#define PLAYER_Z 4.0f
#define STAR_COUNT 120

typedef struct Position { float x, z; } Position;
typedef struct Velocity { float x, z; } Velocity;
typedef struct Collider { float radius; } Collider;
typedef struct Health { int hp; } Health;
typedef struct Lifetime { float seconds; } Lifetime;
typedef struct Renderable {
    uint8_t mesh;
    uint8_t kind;
    float scale;
    float yaw;
} Renderable;
typedef struct EnemyBrain {
    float base_x;
    float phase;
    float sway;
    float fire_cd;
    uint8_t type;
} EnemyBrain;

typedef struct PlayerTag PlayerTag;
typedef struct EnemyTag EnemyTag;
typedef struct PlayerShotTag PlayerShotTag;
typedef struct EnemyShotTag EnemyShotTag;
typedef struct ParticleTag ParticleTag;

CECS_COMPONENT_DECLARE(Position);
CECS_COMPONENT_DECLARE(Velocity);
CECS_COMPONENT_DECLARE(Collider);
CECS_COMPONENT_DECLARE(Health);
CECS_COMPONENT_DECLARE(Lifetime);
CECS_COMPONENT_DECLARE(Renderable);
CECS_COMPONENT_DECLARE(EnemyBrain);
CECS_TAG_DECLARE(PlayerTag);
CECS_TAG_DECLARE(EnemyTag);
CECS_TAG_DECLARE(PlayerShotTag);
CECS_TAG_DECLARE(EnemyShotTag);
CECS_TAG_DECLARE(ParticleTag);

enum {
    RK_MESH,
    RK_PLAYER_SHOT,
    RK_ENEMY_SHOT,
    RK_PARTICLE
};

typedef struct InputState {
    bool keys[512];
    bool mouse_down;
    float mouse_x;
    float mouse_y;
    bool touch_move_down;
    uintptr_t touch_move_id;
    float touch_move_origin_x;
    float touch_move_origin_y;
    float touch_move_x;
    float touch_move_y;
    bool touch_fire_down;
    uintptr_t touch_fire_id;
    bool fire_pressed;
} InputState;

typedef struct Star {
    float x;
    float z;
    float size;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Star;

typedef struct Game {
    cecs_world world;
    cecs_schedule schedule;
    cecs_entity player;
    InputState input;
    Star stars[STAR_COUNT];
    uint32_t rng;
    float dt;
    float time;
    float scroll;
    float spawn_timer;
    float shot_timer;
    float invuln_timer;
    int score;
    int lives;
    bool game_over;
    sgl_pipeline solid_pip;
    sgl_pipeline alpha_pip;
} Game;

static Game g;

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint32_t rnd_u32(void) {
    uint32_t x = g.rng ? g.rng : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g.rng = x;
    return x;
}

static float rnd_range(float lo, float hi) {
    return lo + (hi - lo) * ((float)(rnd_u32() & 0xffffu) / 65535.0f);
}

static float dist2(float ax, float az, float bx, float bz) {
    float dx = ax - bx;
    float dz = az - bz;
    return dx * dx + dz * dz;
}

static void add_component_types(cecs_world *world) {
    cecs_type_registration types[] = {
        CECS_COMPONENT_TYPE(Position),
        CECS_COMPONENT_TYPE(Velocity),
        CECS_COMPONENT_TYPE(Collider),
        CECS_COMPONENT_TYPE(Health),
        CECS_COMPONENT_TYPE(Lifetime),
        CECS_COMPONENT_TYPE(Renderable),
        CECS_COMPONENT_TYPE(EnemyBrain),
        CECS_TAG_TYPE(PlayerTag),
        CECS_TAG_TYPE(EnemyTag),
        CECS_TAG_TYPE(PlayerShotTag),
        CECS_TAG_TYPE(EnemyShotTag),
        CECS_TAG_TYPE(ParticleTag),
    };
    (void)CECS_REGISTER_TYPES(world, types);
}

static cecs_entity spawn_mesh(uint8_t mesh, float x, float z, float scale, float yaw) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { x, z };
    Velocity v = { 0.0f, 0.0f };
    Collider c = { scale * trad_meshes[mesh].radius * 0.78f };
    Renderable r = { mesh, RK_MESH, scale, yaw };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Renderable, &r);
    return e;
}

static void spawn_burst(float x, float z, int count, uint8_t r, uint8_t gg, uint8_t b) {
    for (int i = 0; i < count; i++) {
        cecs_entity e = cecs_spawn(&g.world);
        float a = rnd_range(0.0f, 2.0f * PI);
        float speed = rnd_range(2.0f, 6.0f);
        Position p = { x, z };
        Velocity v = { cosf(a) * speed, sinf(a) * speed };
        Lifetime lt = { rnd_range(0.18f, 0.42f) };
        Collider c = { 0.08f };
        Renderable rr = { 0, RK_PARTICLE, rnd_range(0.08f, 0.18f), 0.0f };
        (void)CECS_ADD(&g.world, e, Position, &p);
        (void)CECS_ADD(&g.world, e, Velocity, &v);
        (void)CECS_ADD(&g.world, e, Lifetime, &lt);
        (void)CECS_ADD(&g.world, e, Collider, &c);
        (void)CECS_ADD(&g.world, e, Renderable, &rr);
        (void)CECS_ADD(&g.world, e, ParticleTag, NULL);
        (void)r;
        (void)gg;
        (void)b;
    }
}

static void spawn_player(void) {
    g.player = spawn_mesh(TRAD_MESH_CRAFT_RACER, 0.0f, PLAYER_Z, 1.45f, 0.0f);
    Health h = { 3 };
    (void)CECS_ADD(&g.world, g.player, Health, &h);
    (void)CECS_ADD(&g.world, g.player, PlayerTag, NULL);
}

static void spawn_player_shot(float x, float z, float vx) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { x, z };
    Velocity v = { vx, 20.0f };
    Collider c = { 0.16f };
    Lifetime lt = { 1.8f };
    Renderable rr = { 0, RK_PLAYER_SHOT, 0.34f, 0.0f };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    (void)CECS_ADD(&g.world, e, Renderable, &rr);
    (void)CECS_ADD(&g.world, e, PlayerShotTag, NULL);
}

static void spawn_enemy_shot(float x, float z, float tx, float tz) {
    cecs_entity e = cecs_spawn(&g.world);
    float dx = tx - x;
    float dz = tz - z;
    float mag = sqrtf(dx * dx + dz * dz);
    if (mag < 0.001f) {
        dx = 0.0f;
        dz = -1.0f;
        mag = 1.0f;
    }
    Position p = { x, z };
    Velocity v = { dx / mag * 6.7f, dz / mag * 6.7f };
    Collider c = { 0.22f };
    Lifetime lt = { 4.2f };
    Renderable rr = { 0, RK_ENEMY_SHOT, 0.28f, atan2f(v.x, v.z) };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    (void)CECS_ADD(&g.world, e, Renderable, &rr);
    (void)CECS_ADD(&g.world, e, EnemyShotTag, NULL);
}

static void spawn_enemy(float x, float z, int type) {
    static const uint8_t meshes[] = {
        TRAD_MESH_CRAFT_SPEEDERA,
        TRAD_MESH_CRAFT_SPEEDERB,
        TRAD_MESH_CRAFT_CARGOA,
        TRAD_MESH_METEOR_DETAILED
    };
    float scale = (type == 2) ? 1.55f : ((type == 3) ? 1.35f : 1.18f);
    cecs_entity e = spawn_mesh(meshes[type & 3], x, z, scale, PI);
    Velocity *v = CECS_GET(&g.world, e, Velocity);
    if (v) {
        v->z = (type == 3) ? -2.7f : -3.3f - (float)(type & 1) * 0.9f;
    }
    Health h = { (type == 2) ? 4 : ((type == 3) ? 3 : 2) };
    EnemyBrain brain = { x, rnd_range(0.0f, 6.0f), rnd_range(0.35f, 1.15f), rnd_range(0.9f, 2.0f), (uint8_t)type };
    (void)CECS_ADD(&g.world, e, Health, &h);
    (void)CECS_ADD(&g.world, e, EnemyBrain, &brain);
    (void)CECS_ADD(&g.world, e, EnemyTag, NULL);
}

static void spawn_wave(void) {
    int pattern = (int)(rnd_u32() % 5u);
    if (pattern == 0) {
        float x = rnd_range(-6.8f, 6.8f);
        for (int i = 0; i < 3; i++) spawn_enemy(x + (float)(i - 1) * 1.35f, WORLD_H + (float)i * 0.75f, i & 1);
    } else if (pattern == 1) {
        float side = (rnd_u32() & 1u) ? -1.0f : 1.0f;
        for (int i = 0; i < 4; i++) spawn_enemy(side * (7.2f - (float)i * 1.5f), WORLD_H + (float)i * 0.85f, 1);
    } else if (pattern == 2) {
        spawn_enemy(rnd_range(-6.2f, 6.2f), WORLD_H + 0.5f, 2);
    } else if (pattern == 3) {
        for (int i = 0; i < 2; i++) spawn_enemy(rnd_range(-7.5f, 7.5f), WORLD_H + (float)i * 1.8f, 3);
    } else {
        spawn_enemy(-4.8f, WORLD_H + 0.3f, 0);
        spawn_enemy(4.8f, WORLD_H + 0.3f, 0);
        spawn_enemy(0.0f, WORLD_H + 1.4f, 2);
    }
}

static void reset_game(void) {
    memset(&g.world, 0, sizeof(g.world));
    cecs_world_init(&g.world);
    add_component_types(&g.world);
    g.player = cecs_entity_null();
    g.dt = 1.0f / 60.0f;
    g.time = 0.0f;
    g.scroll = 0.0f;
    g.spawn_timer = 0.35f;
    g.shot_timer = 0.0f;
    g.invuln_timer = 1.2f;
    g.score = 0;
    g.lives = 2;
    g.game_over = false;
    spawn_player();
}

static void collect_input(float *out_x, float *out_z, bool *out_fire) {
    float ix = 0.0f;
    float iz = 0.0f;
    if (g.input.keys[SAPP_KEYCODE_A] || g.input.keys[SAPP_KEYCODE_LEFT]) ix -= 1.0f;
    if (g.input.keys[SAPP_KEYCODE_D] || g.input.keys[SAPP_KEYCODE_RIGHT]) ix += 1.0f;
    if (g.input.keys[SAPP_KEYCODE_W] || g.input.keys[SAPP_KEYCODE_UP]) iz += 1.0f;
    if (g.input.keys[SAPP_KEYCODE_S] || g.input.keys[SAPP_KEYCODE_DOWN]) iz -= 1.0f;
    if (g.input.touch_move_down) {
        float dx = (g.input.touch_move_x - g.input.touch_move_origin_x) / 64.0f;
        float dz = (g.input.touch_move_origin_y - g.input.touch_move_y) / 64.0f;
        ix += clampf(dx, -1.0f, 1.0f);
        iz += clampf(dz, -1.0f, 1.0f);
    }
    if (g.input.mouse_down) {
        Position *p = CECS_GET(&g.world, g.player, Position);
        if (p) {
            float w = sapp_widthf();
            float h = sapp_heightf();
            float view_h = WORLD_H + 2.0f;
            float view_w = view_h * (w / (h > 1.0f ? h : 1.0f));
            float tx = (g.input.mouse_x / w - 0.5f) * view_w;
            float tz = (1.0f - g.input.mouse_y / h) * (WORLD_H + 1.5f) - 0.75f;
            ix += clampf((tx - p->x) * 0.38f, -1.0f, 1.0f);
            iz += clampf((tz - p->z) * 0.38f, -1.0f, 1.0f);
        }
    }
    float mag = sqrtf(ix * ix + iz * iz);
    if (mag > 1.0f) {
        ix /= mag;
        iz /= mag;
    }
    *out_x = ix;
    *out_z = iz;
    *out_fire = g.input.keys[SAPP_KEYCODE_SPACE] || g.input.keys[SAPP_KEYCODE_Z] ||
                g.input.keys[SAPP_KEYCODE_X] || g.input.keys[SAPP_KEYCODE_ENTER] ||
                g.input.mouse_down || g.input.touch_fire_down;
}

static void player_system(cecs_world *world, void *ctx) {
    (void)ctx;
    float ix, iz;
    bool fire;
    collect_input(&ix, &iz, &fire);
    if (!cecs_is_alive(world, g.player)) return;
    Position *p = CECS_GET(world, g.player, Position);
    Renderable *r = CECS_GET(world, g.player, Renderable);
    if (!p || !r) return;
    p->x = clampf(p->x + ix * 10.8f * g.dt, -8.1f, 8.1f);
    p->z = clampf(p->z + iz * 9.6f * g.dt, 1.2f, 12.4f);
    r->yaw = clampf(-ix * 0.35f, -0.42f, 0.42f);
    if (g.shot_timer > 0.0f) g.shot_timer -= g.dt;
    if (fire && g.shot_timer <= 0.0f) {
        spawn_player_shot(p->x - 0.42f, p->z + 0.95f, -1.0f);
        spawn_player_shot(p->x + 0.42f, p->z + 0.95f, 1.0f);
        spawn_player_shot(p->x, p->z + 1.15f, 0.0f);
        g.shot_timer = 0.105f;
    }
}

static void enemy_system(cecs_world *world, void *ctx) {
    (void)ctx;
    Position *player_pos = CECS_GET(world, g.player, Position);
    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Velocity), CECS_COMPONENT_ID(EnemyBrain), CECS_COMPONENT_ID(Renderable) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Velocity *v = CECS_QUERY_GET_FAST(&_q, Velocity);
            EnemyBrain *brain = CECS_QUERY_GET_FAST(&_q, EnemyBrain);
            Renderable *rr = CECS_QUERY_GET_FAST(&_q, Renderable);
            brain->phase += g.dt * (1.4f + (float)brain->type * 0.2f);
            p->x = brain->base_x + sinf(brain->phase) * brain->sway;
            rr->yaw = PI + sinf(brain->phase * 1.7f) * 0.12f;
            if (brain->type == 3) {
                rr->yaw += g.time * 0.8f;
                v->x = sinf(brain->phase * 0.8f) * 0.6f;
            }
            brain->fire_cd -= g.dt;
            if (player_pos && brain->type != 3 && brain->fire_cd <= 0.0f && p->z < WORLD_H - 3.0f && p->z > 9.0f) {
                spawn_enemy_shot(p->x, p->z - 0.5f, player_pos->x, player_pos->z);
                brain->fire_cd = rnd_range(1.2f, 2.4f);
            }
        });
}

static void spawn_system(cecs_world *world, void *ctx) {
    (void)world;
    (void)ctx;
    g.spawn_timer -= g.dt;
    if (g.spawn_timer <= 0.0f) {
        spawn_wave();
        g.spawn_timer = clampf(1.05f - g.time * 0.006f, 0.42f, 1.05f);
    }
}

static void movement_system(cecs_world *world, void *ctx) {
    (void)ctx;
    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Velocity) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Velocity *v = CECS_QUERY_GET_FAST(&_q, Velocity);
            p->x += v->x * g.dt;
            p->z += v->z * g.dt;
        });
}

static void lifetime_system(cecs_world *world, void *ctx) {
    (void)ctx;
    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Lifetime) }), {
            Lifetime *lt = CECS_QUERY_GET_FAST(&_q, Lifetime);
            lt->seconds -= g.dt;
            if (lt->seconds <= 0.0f) {
                (void)cecs_despawn(world, _q.entity);
            }
        });
}

static void cleanup_system(cecs_world *world, void *ctx) {
    (void)ctx;
    CECS_QUERY_EACH_WITHOUT(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position) }),
        ((cecs_component_id[]){ CECS_COMPONENT_ID(PlayerTag) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            if (p->z < -4.0f || p->z > WORLD_H + 7.0f || p->x < -12.0f || p->x > 12.0f) {
                (void)cecs_despawn(world, _q.entity);
            }
        });
}

static void hit_player(cecs_world *world, float x, float z) {
    if (g.invuln_timer > 0.0f || !cecs_is_alive(world, g.player)) return;
    Health *h = CECS_GET(world, g.player, Health);
    Position *p = CECS_GET(world, g.player, Position);
    if (!h || !p) return;
    h->hp--;
    g.invuln_timer = 1.15f;
    spawn_burst(x, z, 18, 255, 170, 80);
    if (h->hp <= 0) {
        g.lives--;
        if (g.lives < 0) {
            g.game_over = true;
            spawn_burst(p->x, p->z, 48, 255, 210, 110);
            (void)cecs_despawn(world, g.player);
        } else {
            h->hp = 3;
            p->x = 0.0f;
            p->z = PLAYER_Z;
        }
    }
}

static void collision_system(cecs_world *world, void *ctx) {
    (void)ctx;
    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(PlayerShotTag) }), {
            Position *shot_p = CECS_QUERY_GET_FAST(&_q, Position);
            Collider *shot_c = CECS_QUERY_GET_FAST(&_q, Collider);
            cecs_entity shot_e = _q.entity;
            bool consumed = false;
            CECS_QUERY_EACH(world,
                ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(Health), CECS_COMPONENT_ID(EnemyTag) }), {
                    if (consumed) { continue; }
                    Position *enemy_p = CECS_QUERY_GET_FAST(&_q, Position);
                    Collider *enemy_c = CECS_QUERY_GET_FAST(&_q, Collider);
                    Health *enemy_h = CECS_QUERY_GET_FAST(&_q, Health);
                    float rr = shot_c->radius + enemy_c->radius;
                    if (dist2(shot_p->x, shot_p->z, enemy_p->x, enemy_p->z) <= rr * rr) {
                        enemy_h->hp--;
                        consumed = true;
                        (void)cecs_despawn(world, shot_e);
                        spawn_burst(shot_p->x, shot_p->z, 7, 80, 210, 255);
                        if (enemy_h->hp <= 0) {
                            g.score += 100;
                            spawn_burst(enemy_p->x, enemy_p->z, 28, 255, 155, 70);
                            (void)cecs_despawn(world, _q.entity);
                        }
                    }
                });
        });

    Position *player_p = CECS_GET(world, g.player, Position);
    Collider *player_c = CECS_GET(world, g.player, Collider);
    if (!player_p || !player_c) return;

    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(EnemyShotTag) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Collider *c = CECS_QUERY_GET_FAST(&_q, Collider);
            float rr = player_c->radius + c->radius;
            if (dist2(player_p->x, player_p->z, p->x, p->z) <= rr * rr) {
                (void)cecs_despawn(world, _q.entity);
                hit_player(world, p->x, p->z);
            }
        });

    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(EnemyTag) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Collider *c = CECS_QUERY_GET_FAST(&_q, Collider);
            float rr = player_c->radius + c->radius;
            if (dist2(player_p->x, player_p->z, p->x, p->z) <= rr * rr) {
                spawn_burst(p->x, p->z, 24, 255, 155, 70);
                (void)cecs_despawn(world, _q.entity);
                hit_player(world, player_p->x, player_p->z);
            }
        });
}

static void init_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x = rnd_range(-13.5f, 13.5f);
        g.stars[i].z = rnd_range(-2.0f, WORLD_H + 4.0f);
        g.stars[i].size = rnd_range(0.025f, 0.09f);
        uint8_t shade = (uint8_t)rnd_range(115.0f, 255.0f);
        g.stars[i].r = shade;
        g.stars[i].g = (uint8_t)clampf((float)shade + rnd_range(-10.0f, 22.0f), 80.0f, 255.0f);
        g.stars[i].b = 255;
    }
}

static void tri_v(float x, float y, float z, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    sgl_v3f_c4b(x, y, z, r, gg, b, a);
}

static void draw_quad3(float x0, float z0, float x1, float z1, float y, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    sgl_begin_triangles();
    tri_v(x0, y, z0, r, gg, b, a);
    tri_v(x1, y, z0, r, gg, b, a);
    tri_v(x1, y, z1, r, gg, b, a);
    tri_v(x0, y, z0, r, gg, b, a);
    tri_v(x1, y, z1, r, gg, b, a);
    tri_v(x0, y, z1, r, gg, b, a);
    sgl_end();
}

static void draw_box(float x, float z, float sx, float sz, float sy, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    float x0 = x - sx * 0.5f, x1 = x + sx * 0.5f;
    float z0 = z - sz * 0.5f, z1 = z + sz * 0.5f;
    float y0 = 0.04f, y1 = y0 + sy;
    sgl_begin_triangles();
    tri_v(x0, y1, z0, r, gg, b, a); tri_v(x1, y1, z0, r, gg, b, a); tri_v(x1, y1, z1, r, gg, b, a);
    tri_v(x0, y1, z0, r, gg, b, a); tri_v(x1, y1, z1, r, gg, b, a); tri_v(x0, y1, z1, r, gg, b, a);
    tri_v(x0, y0, z0, r / 2, gg / 2, b / 2, a); tri_v(x1, y0, z0, r / 2, gg / 2, b / 2, a); tri_v(x1, y1, z0, r, gg, b, a);
    tri_v(x0, y0, z0, r / 2, gg / 2, b / 2, a); tri_v(x1, y1, z0, r, gg, b, a); tri_v(x0, y1, z0, r, gg, b, a);
    sgl_end();
}

static void draw_mesh(uint8_t mesh_id, float x, float z, float scale, float yaw) {
    const TradMesh *mesh = &trad_meshes[mesh_id];
    sgl_push_matrix();
    sgl_translate(x, 0.0f, z);
    sgl_rotate(yaw, 0.0f, 1.0f, 0.0f);
    sgl_scale(scale, scale, scale);
    sgl_begin_triangles();
    for (int i = 0; i < mesh->vertex_count; i++) {
        const TradVertex *v = &mesh->vertices[i];
        sgl_v3f_c4b(v->x, v->y, v->z, v->r, v->g, v->b, v->a);
    }
    sgl_end();
    sgl_pop_matrix();
}

static void draw_disc3(float x, float z, float radius, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    const int segments = 18;
    sgl_begin_triangles();
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / (float)segments * 2.0f * PI;
        float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;
        tri_v(x, 0.015f, z, r, gg, b, a);
        tri_v(x + cosf(a0) * radius, 0.015f, z + sinf(a0) * radius, r, gg, b, a);
        tri_v(x + cosf(a1) * radius, 0.015f, z + sinf(a1) * radius, r, gg, b, a);
    }
    sgl_end();
}

static void draw_world(void) {
    float aspect = sapp_widthf() / (sapp_heightf() > 1.0f ? sapp_heightf() : 1.0f);
    float half_x = (WORLD_H + 1.5f) * aspect * 0.5f;

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(-half_x, half_x, -1.25f, WORLD_H + 1.25f, -64.0f, 64.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
    sgl_lookat(0.0f, 32.0f, 15.0f, 0.0f, 0.0f, 15.0f, 0.0f, 0.0f, 1.0f);

    sgl_load_pipeline(g.alpha_pip);
    draw_quad3(-half_x - 2.0f, -4.0f, half_x + 2.0f, WORLD_H + 6.0f, -0.08f, 5, 8, 22, 255);
    draw_quad3(-3.6f, -4.0f, 3.6f, WORLD_H + 6.0f, -0.07f, 16, 18, 32, 185);
    draw_quad3(-0.035f, -4.0f, 0.035f, WORLD_H + 6.0f, -0.06f, 90, 130, 170, 85);
    for (int i = 0; i < STAR_COUNT; i++) {
        float z = fmodf(g.stars[i].z - g.scroll * (0.32f + g.stars[i].size * 4.0f), WORLD_H + 6.0f);
        if (z < -3.0f) z += WORLD_H + 6.0f;
        float s = g.stars[i].size;
        draw_quad3(g.stars[i].x - s, z - s, g.stars[i].x + s, z + s, -0.045f, g.stars[i].r, g.stars[i].g, g.stars[i].b, 170);
    }
    for (int i = 0; i < 9; i++) {
        float z = fmodf((float)i * 4.2f - g.scroll * 1.8f, WORLD_H + 6.0f);
        if (z < -3.0f) z += WORLD_H + 6.0f;
        draw_quad3(-8.7f, z, -7.7f, z + 1.4f, -0.055f, 27, 39, 62, 140);
        draw_quad3(7.7f, z + 1.8f, 8.7f, z + 3.2f, -0.055f, 27, 39, 62, 140);
    }

    CECS_QUERY_EACH(&g.world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Renderable), CECS_COMPONENT_ID(Collider) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Renderable *rr = CECS_QUERY_GET_FAST(&_q, Renderable);
            Collider *c = CECS_QUERY_GET_FAST(&_q, Collider);
            if (rr->kind == RK_MESH) {
                uint8_t alpha = (CECS_QUERY_HAS_FAST(&_q, PlayerTag) && g.invuln_timer > 0.0f) ? 70 : 95;
                draw_disc3(p->x, p->z, c->radius * 1.05f, 0, 0, 0, alpha);
            }
        });

    sgl_load_pipeline(g.solid_pip);
    CECS_QUERY_EACH(&g.world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Renderable) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Renderable *rr = CECS_QUERY_GET_FAST(&_q, Renderable);
            if (rr->kind == RK_MESH) {
                if (!CECS_QUERY_HAS_FAST(&_q, PlayerTag) || g.invuln_timer <= 0.0f || ((int)(g.time * 18.0f) & 1) == 0) {
                    draw_mesh(rr->mesh, p->x, p->z, rr->scale, rr->yaw);
                }
            } else if (rr->kind == RK_PLAYER_SHOT) {
                draw_box(p->x, p->z + 0.18f, 0.12f, 0.78f, 0.16f, 80, 218, 255, 255);
            } else if (rr->kind == RK_ENEMY_SHOT) {
                draw_box(p->x, p->z, 0.28f, 0.28f, 0.18f, 255, 84, 54, 255);
            } else if (rr->kind == RK_PARTICLE) {
                draw_box(p->x, p->z, rr->scale, rr->scale, rr->scale, 255, 160, 64, 220);
            }
        });
}

static void draw_disc2(float cx, float cy, float radius, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    const int segments = 28;
    sgl_begin_triangles();
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / (float)segments * 2.0f * PI;
        float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;
        sgl_v2f_c4b(cx, cy, r, gg, b, a);
        sgl_v2f_c4b(cx + cosf(a0) * radius, cy + sinf(a0) * radius, r, gg, b, a);
        sgl_v2f_c4b(cx + cosf(a1) * radius, cy + sinf(a1) * radius, r, gg, b, a);
    }
    sgl_end();
}

static void draw_ring2(float cx, float cy, float radius, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    const int segments = 48;
    sgl_begin_line_strip();
    for (int i = 0; i <= segments; i++) {
        float aa = (float)i / (float)segments * 2.0f * PI;
        sgl_v2f_c4b(cx + cosf(aa) * radius, cy + sinf(aa) * radius, r, gg, b, a);
    }
    sgl_end();
}

static void draw_ui(void) {
    float w = sapp_widthf();
    float h = sapp_heightf();
    sgl_load_pipeline(g.alpha_pip);
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    bool show_touch_controls = (w < h * 0.92f) || g.input.touch_move_down || g.input.touch_fire_down;
    if (show_touch_controls) {
        float pad_r = clampf(w * 0.085f, 46.0f, 82.0f);
        float left_x = pad_r + 26.0f;
        float left_y = h - pad_r - 24.0f;
        float right_x = w - pad_r - 30.0f;
        float right_y = h - pad_r - 24.0f;
        draw_disc2(left_x, left_y, pad_r, 42, 61, 84, 58);
        draw_ring2(left_x, left_y, pad_r, 108, 154, 205, 120);
        if (g.input.touch_move_down) {
            float dx = clampf(g.input.touch_move_x - g.input.touch_move_origin_x, -pad_r, pad_r);
            float dy = clampf(g.input.touch_move_y - g.input.touch_move_origin_y, -pad_r, pad_r);
            draw_disc2(left_x + dx * 0.65f, left_y + dy * 0.65f, pad_r * 0.34f, 120, 190, 255, 135);
        }
        draw_disc2(right_x, right_y, pad_r * 0.84f, g.input.touch_fire_down || g.input.mouse_down ? 255 : 95, 70, 56, g.input.touch_fire_down || g.input.mouse_down ? 125 : 58);
        draw_ring2(right_x, right_y, pad_r * 0.84f, 255, 135, 100, 130);
    }

    float text_scale = clampf(sapp_dpi_scale(), 1.0f, 2.0f);
    float tw = w / text_scale;
    float th = h / text_scale;
    sdtx_canvas(tw, th);
    sdtx_font(0);
    sdtx_origin(12.0f, 12.0f);
    sdtx_color3b(210, 232, 255);
    sdtx_pos(0.0f, 0.0f);
    sdtx_printf("SCORE %06d", g.score);
    sdtx_pos(0.0f, 1.3f);
    Health *hp = CECS_GET(&g.world, g.player, Health);
    sdtx_printf("LIVES %d  ARMOR %d", g.lives, hp ? hp->hp : 0);
    if (g.game_over) {
        sdtx_origin(tw * 0.5f - 86.0f, th * 0.48f);
        sdtx_color3b(255, 210, 150);
        sdtx_pos(0.0f, 0.0f);
        sdtx_puts("TRAD STRIKE");
        sdtx_pos(0.0f, 1.5f);
        sdtx_color3b(165, 210, 255);
        sdtx_puts("PRESS FIRE");
    }
}

static void init(void) {
    g.rng = 0x9146b72du;
    sg_environment env = sglue_environment();
    sg_setup(&(sg_desc){
        .environment = env,
        .logger.func = slog_func,
    });
    sgl_setup(&(sgl_desc_t){
        .max_vertices = 180000,
        .max_commands = 32768,
        .logger.func = slog_func,
        .color_format = env.defaults.color_format,
        .depth_format = env.defaults.depth_format,
        .sample_count = env.defaults.sample_count,
    });
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_kc853(),
        .context = {
            .color_format = env.defaults.color_format,
            .depth_format = env.defaults.depth_format,
            .sample_count = env.defaults.sample_count,
        },
        .logger.func = slog_func,
    });
    g.solid_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .depth = { .write_enabled = true, .compare = SG_COMPAREFUNC_LESS_EQUAL },
        .cull_mode = SG_CULLMODE_NONE,
    });
    g.alpha_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .depth = { .write_enabled = false, .compare = SG_COMPAREFUNC_ALWAYS },
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
        .cull_mode = SG_CULLMODE_NONE,
    });

    cecs_schedule_init(&g.schedule);
    (void)cecs_schedule_add(&g.schedule, spawn_system, &g);
    (void)cecs_schedule_add(&g.schedule, player_system, &g);
    (void)cecs_schedule_add(&g.schedule, enemy_system, &g);
    (void)cecs_schedule_add(&g.schedule, movement_system, &g);
    (void)cecs_schedule_add(&g.schedule, collision_system, &g);
    (void)cecs_schedule_add(&g.schedule, lifetime_system, &g);
    (void)cecs_schedule_add(&g.schedule, cleanup_system, &g);
    init_stars();
    reset_game();
}

static void frame(void) {
    g.dt = (float)sapp_frame_duration();
    g.dt = clampf(g.dt, 1.0f / 120.0f, 1.0f / 30.0f);
    if (g.input.fire_pressed && g.game_over) {
        reset_game();
    }
    g.input.fire_pressed = false;
    if (!g.game_over) {
        g.time += g.dt;
        g.scroll += g.dt * 5.2f;
        if (g.invuln_timer > 0.0f) g.invuln_timer -= g.dt;
        (void)cecs_schedule_run(&g.schedule, &g.world);
    } else {
        g.scroll += g.dt * 1.6f;
    }

    sg_pass_action action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.015f, 0.020f, 0.050f, 1.0f },
        },
        .depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = 1.0f,
        },
    };
    sg_begin_pass(&(sg_pass){ .action = action, .swapchain = sglue_swapchain() });
    draw_world();
    draw_ui();
    sgl_draw();
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sdtx_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

static void touch_begin_or_move(const sapp_event *ev) {
    float half = sapp_widthf() * 0.5f;
    for (int i = 0; i < ev->num_touches; i++) {
        const sapp_touchpoint *t = &ev->touches[i];
        if (t->pos_x < half) {
            if (!g.input.touch_move_down || g.input.touch_move_id == t->identifier) {
                if (!g.input.touch_move_down) {
                    g.input.touch_move_origin_x = t->pos_x;
                    g.input.touch_move_origin_y = t->pos_y;
                    g.input.touch_move_id = t->identifier;
                }
                g.input.touch_move_down = true;
                g.input.touch_move_x = t->pos_x;
                g.input.touch_move_y = t->pos_y;
            }
        } else {
            g.input.touch_fire_down = true;
            g.input.touch_fire_id = t->identifier;
            g.input.fire_pressed = true;
        }
    }
}

static void touch_end_or_cancel(const sapp_event *ev) {
    for (int i = 0; i < ev->num_touches; i++) {
        const sapp_touchpoint *t = &ev->touches[i];
        if (g.input.touch_move_down && g.input.touch_move_id == t->identifier) {
            g.input.touch_move_down = false;
        }
        if (g.input.touch_fire_down && g.input.touch_fire_id == t->identifier) {
            g.input.touch_fire_down = false;
        }
    }
    if (ev->num_touches == 0) {
        g.input.touch_move_down = false;
        g.input.touch_fire_down = false;
    }
}

static void event(const sapp_event *ev) {
    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (ev->key_code >= 0 && ev->key_code < (sapp_keycode)CECS_ARRAY_COUNT(g.input.keys)) {
            g.input.keys[ev->key_code] = true;
        }
        if (!ev->key_repeat && (ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_Z ||
                                ev->key_code == SAPP_KEYCODE_X || ev->key_code == SAPP_KEYCODE_ENTER)) {
            g.input.fire_pressed = true;
        }
        if (!ev->key_repeat && ev->key_code == SAPP_KEYCODE_R) {
            reset_game();
        }
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        if (ev->key_code >= 0 && ev->key_code < (sapp_keycode)CECS_ARRAY_COUNT(g.input.keys)) {
            g.input.keys[ev->key_code] = false;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.input.mouse_down = true;
            g.input.mouse_x = ev->mouse_x;
            g.input.mouse_y = ev->mouse_y;
            g.input.fire_pressed = true;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.input.mouse_down = false;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        g.input.mouse_x = ev->mouse_x;
        g.input.mouse_y = ev->mouse_y;
        break;
    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
        touch_begin_or_move(ev);
        break;
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        touch_end_or_cancel(ev);
        break;
    default:
        break;
    }
}

sapp_desc sokol_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 960,
        .height = 1280,
        .sample_count = 4,
        .high_dpi = true,
        .window_title = "TRAD Strike",
        .icon.sokol_default = true,
        .logger.func = slog_func,
        .html5 = {
            .canvas_selector = "#canvas",
            .canvas_resize = false,
            .bubble_mouse_events = false,
            .bubble_touch_events = false,
            .bubble_key_events = false,
        },
    };
}
