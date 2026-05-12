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

#define CECS_MAX_ENTITIES 960u
#define CECS_MAX_COMPONENTS 24u
#define CECS_MAX_SYSTEMS 8u
#define CECS_COMPONENT_STORAGE_BYTES 131072u
#define CECS_IMPLEMENTATION
#include "cecs.h"

#include "generated_assets.h"

#define PI 3.14159265358979323846f
#define WORLD_W 18.0f
#define WORLD_H 30.0f
#define PLAYER_Z 2.2f
#define PLAYER_MIN_Z -2.0f
#define PLAYER_MAX_Z 32.0f
#define STAR_COUNT 180

typedef struct Position { float x, z; } Position;
typedef struct Velocity { float x, z; } Velocity;
typedef struct Collider { float radius; } Collider;
typedef struct Health { int hp; } Health;
typedef struct Lifetime { float seconds; } Lifetime;
typedef struct Damage { int amount; } Damage;
typedef struct Pickup {
    uint8_t kind;
    float phase;
} Pickup;
typedef struct Renderable {
    uint8_t mesh;
    uint8_t kind;
    float scale;
    float yaw;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    float pulse;
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
typedef struct PickupTag PickupTag;

CECS_COMPONENT_DECLARE(Position);
CECS_COMPONENT_DECLARE(Velocity);
CECS_COMPONENT_DECLARE(Collider);
CECS_COMPONENT_DECLARE(Health);
CECS_COMPONENT_DECLARE(Lifetime);
CECS_COMPONENT_DECLARE(Damage);
CECS_COMPONENT_DECLARE(Pickup);
CECS_COMPONENT_DECLARE(Renderable);
CECS_COMPONENT_DECLARE(EnemyBrain);
CECS_TAG_DECLARE(PlayerTag);
CECS_TAG_DECLARE(EnemyTag);
CECS_TAG_DECLARE(PlayerShotTag);
CECS_TAG_DECLARE(EnemyShotTag);
CECS_TAG_DECLARE(ParticleTag);
CECS_TAG_DECLARE(PickupTag);

enum {
    RK_MESH,
    RK_PLAYER_SHOT,
    RK_ENEMY_SHOT,
    RK_PARTICLE,
    RK_POWERUP
};

enum {
    PICKUP_WEAPON,
    PICKUP_ARMOR,
    PICKUP_SCORE
};

typedef struct InputState {
    bool drag_down;
    bool drag_is_touch;
    uintptr_t drag_touch_id;
    float drag_x;
    float drag_y;
    bool fire_pressed;
} InputState;

typedef struct Star {
    float x;
    float z;
    float size;
    float speed;
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
    float flash;
    float shake;
    int weapon_level;
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

static float bottom_hud_h(void) {
    float h = sapp_heightf();
    float w = sapp_widthf();
    return (w < h * 1.18f) ? clampf(h * 0.17f, 118.0f, 168.0f)
                            : clampf(h * 0.10f, 72.0f, 110.0f);
}

static float gameplay_view_h(void) {
    float h = sapp_heightf();
    if (h <= 240.0f) return h;
    float view_h = h - bottom_hud_h();
    return view_h < 240.0f ? 240.0f : view_h;
}

static float world_half_x(void) {
    float aspect = sapp_widthf() / (gameplay_view_h() > 1.0f ? gameplay_view_h() : 1.0f);
    float half_x = (WORLD_H + 1.5f) * aspect * 0.5f;
    float min_half = WORLD_W * 0.58f;
    return half_x < min_half ? min_half : half_x;
}

static float player_muzzle_forward_world(float shot_scale) {
    float mesh_radius = trad_meshes[TRAD_MESH_CRAFT_RACER].radius * 1.45f;
    float projectile_tail = shot_scale * 1.15f;
    return mesh_radius + projectile_tail + 0.2f;
}

static void add_component_types(cecs_world *world) {
    cecs_type_registration types[] = {
        CECS_COMPONENT_TYPE(Position),
        CECS_COMPONENT_TYPE(Velocity),
        CECS_COMPONENT_TYPE(Collider),
        CECS_COMPONENT_TYPE(Health),
        CECS_COMPONENT_TYPE(Lifetime),
        CECS_COMPONENT_TYPE(Damage),
        CECS_COMPONENT_TYPE(Pickup),
        CECS_COMPONENT_TYPE(Renderable),
        CECS_COMPONENT_TYPE(EnemyBrain),
        CECS_TAG_TYPE(PlayerTag),
        CECS_TAG_TYPE(EnemyTag),
        CECS_TAG_TYPE(PlayerShotTag),
        CECS_TAG_TYPE(EnemyShotTag),
        CECS_TAG_TYPE(ParticleTag),
        CECS_TAG_TYPE(PickupTag),
    };
    (void)CECS_REGISTER_TYPES(world, types);
}

static cecs_entity spawn_mesh(uint8_t mesh, float x, float z, float scale, float yaw) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { x, z };
    Velocity v = { 0.0f, 0.0f };
    Collider c = { scale * trad_meshes[mesh].radius * 0.78f };
    Renderable r = { mesh, RK_MESH, scale, yaw, 255, 255, 255, 255, 0.0f };
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
        v.z += rnd_range(-1.3f, 1.7f);
        Lifetime lt = { rnd_range(0.22f, 0.58f) };
        Collider c = { 0.08f };
        Renderable rr = { 0, RK_PARTICLE, rnd_range(0.08f, 0.22f), 0.0f, r, gg, b, 230, rnd_range(0.4f, 1.2f) };
        (void)CECS_ADD(&g.world, e, Position, &p);
        (void)CECS_ADD(&g.world, e, Velocity, &v);
        (void)CECS_ADD(&g.world, e, Lifetime, &lt);
        (void)CECS_ADD(&g.world, e, Collider, &c);
        (void)CECS_ADD(&g.world, e, Renderable, &rr);
        (void)CECS_ADD(&g.world, e, ParticleTag, NULL);
    }
}

static void spawn_pickup(float x, float z, uint8_t kind) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { x, z };
    Velocity v = { rnd_range(-0.45f, 0.45f), -2.35f };
    Collider c = { 0.56f };
    Lifetime lt = { 9.0f };
    Pickup pickup = { kind, rnd_range(0.0f, 6.0f) };
    Renderable rr;
    if (kind == PICKUP_WEAPON) {
        rr = (Renderable){ 0, RK_POWERUP, 0.48f, 0.0f, 80, 222, 255, 230, pickup.phase };
    } else if (kind == PICKUP_ARMOR) {
        rr = (Renderable){ 0, RK_POWERUP, 0.48f, 0.0f, 115, 255, 145, 230, pickup.phase };
    } else {
        rr = (Renderable){ 0, RK_POWERUP, 0.48f, 0.0f, 255, 205, 92, 230, pickup.phase };
    }
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    (void)CECS_ADD(&g.world, e, Pickup, &pickup);
    (void)CECS_ADD(&g.world, e, Renderable, &rr);
    (void)CECS_ADD(&g.world, e, PickupTag, NULL);
}

static void spawn_player(void) {
    g.player = spawn_mesh(TRAD_MESH_CRAFT_RACER, 0.0f, PLAYER_Z, 1.45f, 0.0f);
    Health h = { 3 };
    (void)CECS_ADD(&g.world, g.player, Health, &h);
    (void)CECS_ADD(&g.world, g.player, PlayerTag, NULL);
}

static void spawn_player_shot(float x, float z, float vx, float vz, float scale, int damage, uint8_t r, uint8_t gg, uint8_t b) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { x, z };
    Velocity v = { vx, vz };
    Collider c = { 0.13f + scale * 0.12f };
    Lifetime lt = { 1.8f };
    Damage dmg = { damage };
    Renderable rr = { 0, RK_PLAYER_SHOT, scale, atan2f(vx, vz), r, gg, b, 255, 0.0f };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    (void)CECS_ADD(&g.world, e, Damage, &dmg);
    (void)CECS_ADD(&g.world, e, Renderable, &rr);
    (void)CECS_ADD(&g.world, e, PlayerShotTag, NULL);
}

static void spawn_player_shot_aligned(const Position *origin, float ship_heading, float side, float forward,
                                      float ship_vx, float ship_vz, float spread_heading, float speed, float scale, int damage,
                                      uint8_t r, uint8_t gg, uint8_t b) {
    float sx = cosf(ship_heading);
    float sz = -sinf(ship_heading);
    float fx = sinf(ship_heading);
    float fz = cosf(ship_heading);
    float vx = ship_vx + sinf(spread_heading) * speed;
    float shot_vz = cosf(spread_heading) * speed;
    float vz = fmaxf(ship_vz + shot_vz, shot_vz * 0.72f);
    spawn_player_shot(origin->x + sx * side + fx * forward,
                      origin->z + sz * side + fz * forward,
                      vx, vz, scale, damage, r, gg, b);
}

static void spawn_enemy_shot_dir(float x, float z, float dx, float dz, float speed, float scale) {
    cecs_entity e = cecs_spawn(&g.world);
    float mag = sqrtf(dx * dx + dz * dz);
    if (mag < 0.001f) {
        dx = 0.0f;
        dz = -1.0f;
        mag = 1.0f;
    }
    Position p = { x, z };
    Velocity v = { dx / mag * speed, dz / mag * speed };
    Collider c = { 0.18f + scale * 0.12f };
    Lifetime lt = { 4.2f };
    Renderable rr = { 0, RK_ENEMY_SHOT, scale, atan2f(v.x, v.z), 255, 92, 62, 255, 0.0f };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    (void)CECS_ADD(&g.world, e, Collider, &c);
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    (void)CECS_ADD(&g.world, e, Renderable, &rr);
    (void)CECS_ADD(&g.world, e, EnemyShotTag, NULL);
}

static void spawn_enemy_shot(float x, float z, float tx, float tz, float speed, float scale) {
    spawn_enemy_shot_dir(x, z, tx - x, tz - z, speed, scale);
}

static void spawn_enemy_spread(float x, float z, float tx, float tz, int count, float spread, float speed) {
    float dx = tx - x;
    float dz = tz - z;
    float base = atan2f(dx, dz);
    float center = (float)(count - 1) * 0.5f;
    for (int i = 0; i < count; i++) {
        float a = base + ((float)i - center) * spread;
        spawn_enemy_shot_dir(x, z, sinf(a), cosf(a), speed, 0.27f);
    }
}

static void spawn_enemy(float x, float z, int type) {
    static const uint8_t meshes[] = {
        TRAD_MESH_CRAFT_SPEEDERA,
        TRAD_MESH_CRAFT_SPEEDERB,
        TRAD_MESH_CRAFT_SPEEDERC,
        TRAD_MESH_METEOR_DETAILED,
        TRAD_MESH_CRAFT_SPEEDERD,
        TRAD_MESH_CRAFT_CARGOB,
        TRAD_MESH_CRAFT_MINER,
        TRAD_MESH_METEOR
    };
    float scale = 1.16f;
    int hp = 2;
    float speed = -3.8f;
    switch (type) {
    case 2: scale = 1.28f; hp = 3; speed = -4.0f; break;
    case 3: scale = 1.45f; hp = 3; speed = -2.85f; break;
    case 4: scale = 1.16f; hp = 2; speed = -5.3f; break;
    case 5: scale = 1.95f; hp = 10; speed = -2.0f; break;
    case 6: scale = 1.38f; hp = 5; speed = -3.0f; break;
    case 7: scale = 2.0f; hp = 6; speed = -2.2f; break;
    default: break;
    }
    cecs_entity e = spawn_mesh(meshes[type & 7], x, z, scale, PI);
    Velocity *v = CECS_GET(&g.world, e, Velocity);
    if (v) {
        v->z = speed;
    }
    Health h = { hp };
    EnemyBrain brain = { x, rnd_range(0.0f, 6.0f), rnd_range(0.35f, type == 5 ? 2.4f : 1.2f), rnd_range(0.65f, 1.75f), (uint8_t)type };
    (void)CECS_ADD(&g.world, e, Health, &h);
    (void)CECS_ADD(&g.world, e, EnemyBrain, &brain);
    (void)CECS_ADD(&g.world, e, EnemyTag, NULL);
}

static void spawn_wave(void) {
    int pattern = (int)(rnd_u32() % 8u);
    if (pattern == 0) {
        float x = rnd_range(-6.8f, 6.8f);
        for (int i = 0; i < 5; i++) spawn_enemy(x + (float)(i - 2) * 1.15f, WORLD_H + fabsf((float)(i - 2)) * 0.7f, i & 1);
    } else if (pattern == 1) {
        float side = (rnd_u32() & 1u) ? -1.0f : 1.0f;
        for (int i = 0; i < 5; i++) spawn_enemy(side * (7.4f - (float)i * 1.45f), WORLD_H + (float)i * 0.75f, 4);
    } else if (pattern == 2) {
        spawn_enemy(rnd_range(-5.8f, 5.8f), WORLD_H + 0.5f, 5);
        spawn_enemy(rnd_range(-7.4f, -3.6f), WORLD_H + 1.0f, 0);
        spawn_enemy(rnd_range(3.6f, 7.4f), WORLD_H + 1.0f, 1);
    } else if (pattern == 3) {
        for (int i = 0; i < 4; i++) spawn_enemy(rnd_range(-7.5f, 7.5f), WORLD_H + (float)i * 1.25f, (i & 1) ? 3 : 7);
    } else if (pattern == 4) {
        spawn_enemy(-4.8f, WORLD_H + 0.3f, 0);
        spawn_enemy(4.8f, WORLD_H + 0.3f, 0);
        spawn_enemy(0.0f, WORLD_H + 1.4f, 6);
    } else if (pattern == 5) {
        for (int i = 0; i < 3; i++) {
            spawn_enemy(-6.2f + (float)i * 1.2f, WORLD_H + (float)i * 0.55f, 2);
            spawn_enemy(6.2f - (float)i * 1.2f, WORLD_H + (float)i * 0.55f, 2);
        }
    } else if (pattern == 6) {
        spawn_enemy(rnd_range(-6.8f, 6.8f), WORLD_H + 0.2f, 6);
        spawn_enemy(rnd_range(-6.8f, 6.8f), WORLD_H + 1.5f, 6);
    } else {
        for (int i = 0; i < 6; i++) spawn_enemy(rnd_range(-7.6f, 7.6f), WORLD_H + (float)i * 0.55f, i & 1);
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
    g.flash = 0.0f;
    g.shake = 0.0f;
    g.weapon_level = 1;
    g.score = 0;
    g.lives = 2;
    g.game_over = false;
    spawn_player();
}

static void drag_target_world(float *out_x, float *out_z) {
    float w = sapp_widthf();
    float h = gameplay_view_h();
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;
    float half_x = world_half_x();
    float ship_offset = clampf(sapp_heightf() * 0.06f, 60.0f, 160.0f);
    float sx = clampf(g.input.drag_x, 0.0f, w);
    float sy = clampf(g.input.drag_y - ship_offset, 0.0f, h);
    *out_x = clampf(half_x - (sx / w) * half_x * 2.0f, -half_x + 0.5f, half_x - 0.5f);
    *out_z = clampf((WORLD_H + 1.25f) - (sy / h) * (WORLD_H + 2.5f), PLAYER_MIN_Z, PLAYER_MAX_Z);
}

static void player_system(cecs_world *world, void *ctx) {
    (void)ctx;
    if (!cecs_is_alive(world, g.player)) return;
    Position *p = CECS_GET(world, g.player, Position);
    Renderable *r = CECS_GET(world, g.player, Renderable);
    if (!p || !r) return;
    float move_x = 0.0f;
    float move_z = 0.0f;
    if (g.input.drag_down) {
        float tx;
        float tz;
        drag_target_world(&tx, &tz);
        float dx = tx - p->x;
        float dz = tz - p->z;
        move_x = dx;
        move_z = dz;
        p->x = tx;
        p->z = tz;
    }
    float ship_vx = move_x / (g.dt > 0.001f ? g.dt : 0.001f);
    float ship_vz = move_z / (g.dt > 0.001f ? g.dt : 0.001f);
    r->yaw = clampf(atan2f(ship_vx, 21.0f), -0.52f, 0.52f);
    if (g.shot_timer > 0.0f) g.shot_timer -= g.dt;
    if (g.input.drag_down && g.shot_timer <= 0.0f) {
        float heading = r->yaw;
        int level = g.weapon_level;
        if (level < 1) level = 1;
        if (level > 4) level = 4;
        float muzzle_34 = player_muzzle_forward_world(0.34f);
        float muzzle_42 = player_muzzle_forward_world(0.42f);
        float muzzle_29 = player_muzzle_forward_world(0.29f);
        float muzzle_52 = player_muzzle_forward_world(0.52f);
        spawn_player_shot_aligned(p, heading, -0.42f, muzzle_34, ship_vx, ship_vz, 0.0f, 34.0f, 0.34f, 1, 82, 220, 255);
        spawn_player_shot_aligned(p, heading, 0.42f, muzzle_34, ship_vx, ship_vz, 0.0f, 34.0f, 0.34f, 1, 82, 220, 255);
        if (level >= 2) {
            spawn_player_shot_aligned(p, heading, 0.0f, muzzle_42, ship_vx, ship_vz, 0.0f, 37.0f, 0.42f, 2, 135, 246, 255);
        }
        if (level >= 3) {
            spawn_player_shot_aligned(p, heading, -0.70f, muzzle_29, ship_vx, ship_vz, -0.12f, 32.0f, 0.29f, 1, 122, 255, 190);
            spawn_player_shot_aligned(p, heading, 0.70f, muzzle_29, ship_vx, ship_vz, 0.12f, 32.0f, 0.29f, 1, 122, 255, 190);
        }
        if (level >= 4) {
            spawn_player_shot_aligned(p, heading, 0.0f, muzzle_52, ship_vx, ship_vz, 0.0f, 40.0f, 0.52f, 3, 255, 228, 132);
        }
        g.shot_timer = 0.12f - (float)(level - 1) * 0.014f;
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
            if (brain->type == 4) {
                p->x = brain->base_x + sinf(brain->phase * 1.9f) * brain->sway * 1.8f;
            } else if (brain->type == 5) {
                p->x = brain->base_x + sinf(brain->phase * 0.55f) * brain->sway;
                if (p->z < 22.5f && p->z > 17.0f) v->z = -0.8f;
            } else if (brain->type == 6) {
                p->x = brain->base_x + sinf(brain->phase * 1.1f) * brain->sway * 0.55f;
            } else {
                p->x = brain->base_x + sinf(brain->phase) * brain->sway;
            }
            rr->yaw = PI + sinf(brain->phase * 1.7f) * 0.16f;
            if (brain->type == 3 || brain->type == 7) {
                rr->yaw += g.time * 0.8f;
                v->x = sinf(brain->phase * 0.8f) * 0.6f;
            }
            brain->fire_cd -= g.dt;
            if (player_pos && brain->type != 3 && brain->type != 7 && brain->fire_cd <= 0.0f && p->z < WORLD_H - 2.0f && p->z > 8.0f) {
                if (brain->type == 5) {
                    spawn_enemy_spread(p->x, p->z - 0.5f, player_pos->x, player_pos->z, 5, 0.18f, 6.0f);
                    brain->fire_cd = rnd_range(0.95f, 1.35f);
                } else if (brain->type == 2 || brain->type == 6) {
                    spawn_enemy_spread(p->x, p->z - 0.5f, player_pos->x, player_pos->z, 3, 0.16f, 6.8f);
                    brain->fire_cd = rnd_range(1.15f, 1.8f);
                } else {
                    spawn_enemy_shot(p->x, p->z - 0.5f, player_pos->x, player_pos->z, 7.2f, 0.28f);
                    brain->fire_cd = rnd_range(1.05f, 2.0f);
                }
            }
        });
}

static void spawn_system(cecs_world *world, void *ctx) {
    (void)world;
    (void)ctx;
    g.spawn_timer -= g.dt;
    if (g.spawn_timer <= 0.0f) {
        spawn_wave();
        g.spawn_timer = clampf(1.05f - g.time * 0.0065f, 0.38f, 1.05f);
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
    g.shake = 0.42f;
    g.flash = 0.55f;
    if (g.weapon_level > 1) g.weapon_level--;
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
            Damage *shot_d = CECS_GET(world, _q.entity, Damage);
            cecs_entity shot_e = _q.entity;
            bool consumed = false;
            CECS_QUERY_EACH(world,
                ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(Health), CECS_COMPONENT_ID(EnemyTag), CECS_COMPONENT_ID(EnemyBrain) }), {
                    if (consumed) { continue; }
                    Position *enemy_p = CECS_QUERY_GET_FAST(&_q, Position);
                    Collider *enemy_c = CECS_QUERY_GET_FAST(&_q, Collider);
                    Health *enemy_h = CECS_QUERY_GET_FAST(&_q, Health);
                    EnemyBrain *brain = CECS_QUERY_GET_FAST(&_q, EnemyBrain);
                    float rr = shot_c->radius + enemy_c->radius;
                    if (dist2(shot_p->x, shot_p->z, enemy_p->x, enemy_p->z) <= rr * rr) {
                        enemy_h->hp -= shot_d ? shot_d->amount : 1;
                        consumed = true;
                        (void)cecs_despawn(world, shot_e);
                        spawn_burst(shot_p->x, shot_p->z, 7, 80, 210, 255);
                        if (enemy_h->hp <= 0) {
                            int value = 100 + (int)brain->type * 35;
                            if (brain->type == 5 || brain->type == 7) value += 280;
                            g.score += value;
                            g.shake = brain->type == 5 ? 0.33f : 0.16f;
                            g.flash = brain->type == 5 ? 0.34f : 0.18f;
                            spawn_burst(enemy_p->x, enemy_p->z, brain->type == 5 ? 46 : 28, 255, 155, 70);
                            uint32_t roll = rnd_u32() % 100u;
                            if (brain->type == 5 || roll < 12u) {
                                spawn_pickup(enemy_p->x, enemy_p->z, (roll < 45u) ? PICKUP_WEAPON : ((roll < 72u) ? PICKUP_ARMOR : PICKUP_SCORE));
                            }
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

    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Collider), CECS_COMPONENT_ID(Pickup), CECS_COMPONENT_ID(PickupTag) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Collider *c = CECS_QUERY_GET_FAST(&_q, Collider);
            Pickup *pickup = CECS_QUERY_GET_FAST(&_q, Pickup);
            float rr = player_c->radius + c->radius;
            if (dist2(player_p->x, player_p->z, p->x, p->z) <= rr * rr) {
                if (pickup->kind == PICKUP_WEAPON) {
                    if (g.weapon_level < 4) g.weapon_level++;
                    g.score += 250;
                    spawn_burst(p->x, p->z, 18, 80, 225, 255);
                } else if (pickup->kind == PICKUP_ARMOR) {
                    Health *hp = CECS_GET(world, g.player, Health);
                    if (hp && hp->hp < 5) hp->hp++;
                    g.score += 150;
                    spawn_burst(p->x, p->z, 18, 120, 255, 150);
                } else {
                    g.score += 750;
                    spawn_burst(p->x, p->z, 22, 255, 210, 95);
                }
                g.flash = 0.22f;
                (void)cecs_despawn(world, _q.entity);
            }
        });
}

static void init_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x = rnd_range(-13.5f, 13.5f);
        g.stars[i].z = rnd_range(-2.0f, WORLD_H + 4.0f);
        g.stars[i].size = rnd_range(0.025f, 0.09f);
        g.stars[i].speed = rnd_range(0.28f, 1.45f);
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

static void draw_box_rot(float x, float z, float sx, float sz, float sy, float yaw, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    sgl_push_matrix();
    sgl_translate(x, 0.0f, z);
    sgl_rotate(yaw, 0.0f, 1.0f, 0.0f);
    draw_box(0.0f, 0.0f, sx, sz, sy, r, gg, b, a);
    sgl_pop_matrix();
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

static void draw_ring3(float x, float z, float radius, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    const int segments = 42;
    sgl_begin_line_strip();
    for (int i = 0; i <= segments; i++) {
        float aa = (float)i / (float)segments * 2.0f * PI;
        sgl_v3f_c4b(x + cosf(aa) * radius, 0.075f, z + sinf(aa) * radius, r, gg, b, a);
    }
    sgl_end();
}

static void draw_world_scenery(float half_x) {
    const float wrap = WORLD_H + 9.0f;
    sgl_load_pipeline(g.alpha_pip);
    for (int i = 0; i < 11; i++) {
        float z = fmodf((float)i * 3.9f - g.scroll * 1.8f, wrap);
        if (z < -4.0f) z += wrap;
        uint8_t pulse = (uint8_t)(90.0f + 55.0f * (0.5f + 0.5f * sinf(g.time * 5.0f + (float)i)));
        draw_quad3(-3.15f, z - 0.75f, -2.88f, z + 0.75f, -0.05f, 42, 130, 190, pulse);
        draw_quad3(2.88f, z - 0.75f, 3.15f, z + 0.75f, -0.05f, 42, 130, 190, pulse);
        draw_quad3(-0.07f, z - 0.48f, 0.07f, z + 0.48f, -0.045f, 150, 230, 255, 72);
    }
    for (int i = 0; i < 8; i++) {
        float z = fmodf((float)i * 6.2f - g.scroll * 0.58f, wrap);
        if (z < -4.0f) z += wrap;
        float x = (i & 1) ? -half_x * 0.58f : half_x * 0.58f;
        float w = 1.2f + (float)(i % 3) * 0.6f;
        draw_disc3(x, z, w, (i & 1) ? 25 : 55, 55, (i & 1) ? 130 : 95, 38);
    }

    sgl_load_pipeline(g.solid_pip);
    for (int i = 0; i < 12; i++) {
        float z = fmodf((float)i * 3.4f - g.scroll * 1.18f, wrap);
        if (z < -4.0f) z += wrap;
        draw_mesh((i % 5 == 0) ? TRAD_MESH_TERRAIN_ROADCROSS : TRAD_MESH_TERRAIN_ROADSTRAIGHT, 0.0f, z, 4.65f, 0.0f);
        if ((i & 1) == 0) {
            draw_mesh(TRAD_MESH_PLATFORM_LONG, -7.2f, z + 0.7f, 2.45f, PI * 0.5f);
            draw_mesh(TRAD_MESH_PLATFORM_LONG, 7.2f, z + 2.0f, 2.45f, -PI * 0.5f);
        }
        if (i % 4 == 1) {
            draw_mesh(TRAD_MESH_HANGAR_SMALLA, -9.1f, z + 1.6f, 1.7f, PI * 0.5f);
        } else if (i % 4 == 2) {
            draw_mesh(TRAD_MESH_GATE_COMPLEX, 8.7f, z + 1.4f, 1.85f, -PI * 0.5f);
        } else if (i % 4 == 3) {
            draw_mesh(TRAD_MESH_SATELLITEDISH_DETAILED, -8.65f, z + 1.2f, 1.7f, sinf(g.time) * 0.35f);
            draw_mesh(TRAD_MESH_MACHINE_GENERATORLARGE, 8.85f, z + 2.4f, 1.65f, 0.0f);
        }
    }
}

static void draw_world(void) {
    float half_x = world_half_x();
    float view_w = sapp_widthf();
    float view_h = gameplay_view_h();
    float shake_x = sinf(g.time * 67.0f) * g.shake;
    float shake_z = cosf(g.time * 53.0f) * g.shake * 0.55f;

    sgl_defaults();
    sgl_viewportf(0.0f, 0.0f, view_w, view_h, true);
    sgl_scissor_rectf(0.0f, 0.0f, view_w, view_h, true);
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(-half_x, half_x, -1.25f, WORLD_H + 1.25f, -64.0f, 64.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
    sgl_lookat(shake_x, 32.0f, shake_z, shake_x * 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    sgl_load_pipeline(g.alpha_pip);
    draw_quad3(-half_x - 2.0f, -4.0f, half_x + 2.0f, WORLD_H + 6.0f, -0.08f, 4, 7, 18, 255);
    draw_quad3(-half_x * 0.95f, -4.0f, half_x * 0.95f, WORLD_H + 6.0f, -0.075f, 12, 13, 26, 155);
    draw_quad3(-4.05f, -4.0f, 4.05f, WORLD_H + 6.0f, -0.069f, 18, 21, 34, 212);
    draw_quad3(-3.55f, -4.0f, 3.55f, WORLD_H + 6.0f, -0.064f, 25, 29, 42, 180);
    draw_quad3(-0.035f, -4.0f, 0.035f, WORLD_H + 6.0f, -0.055f, 90, 160, 210, 90);
    for (int i = 0; i < STAR_COUNT; i++) {
        float z = fmodf(g.stars[i].z - g.scroll * g.stars[i].speed, WORLD_H + 6.0f);
        if (z < -3.0f) z += WORLD_H + 6.0f;
        float s = g.stars[i].size;
        uint8_t a = (uint8_t)clampf(120.0f + g.stars[i].speed * 65.0f, 120.0f, 220.0f);
        draw_quad3(g.stars[i].x - s, z - s, g.stars[i].x + s, z + s, -0.045f, g.stars[i].r, g.stars[i].g, g.stars[i].b, a);
    }
    draw_world_scenery(half_x);

    sgl_load_pipeline(g.alpha_pip);
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

    CECS_QUERY_EACH(&g.world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Renderable) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Renderable *rr = CECS_QUERY_GET_FAST(&_q, Renderable);
            if (rr->kind == RK_PLAYER_SHOT) {
                draw_disc3(p->x, p->z, rr->scale * 1.05f, rr->r, rr->g, rr->b, 92);
            } else if (rr->kind == RK_ENEMY_SHOT) {
                draw_disc3(p->x, p->z, rr->scale * 1.25f, 255, 70, 45, 90);
            } else if (rr->kind == RK_POWERUP) {
                float pulse = 1.0f + 0.18f * sinf(g.time * 7.0f + rr->pulse);
                draw_disc3(p->x, p->z, rr->scale * 1.15f * pulse, rr->r, rr->g, rr->b, 105);
                draw_ring3(p->x, p->z, rr->scale * 1.55f * pulse, rr->r, rr->g, rr->b, 180);
            } else if (CECS_QUERY_HAS_FAST(&_q, PlayerTag) && g.invuln_timer > 0.0f) {
                draw_ring3(p->x, p->z, 1.35f + 0.12f * sinf(g.time * 12.0f), 92, 190, 255, 170);
            }
        });

    sgl_load_pipeline(g.solid_pip);
    CECS_QUERY_EACH(&g.world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Renderable) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Renderable *rr = CECS_QUERY_GET_FAST(&_q, Renderable);
            if (rr->kind == RK_MESH) {
                if (CECS_QUERY_HAS_FAST(&_q, PlayerTag)) {
                    continue;
                }
                if (!CECS_QUERY_HAS_FAST(&_q, PlayerTag) || g.invuln_timer <= 0.0f || ((int)(g.time * 18.0f) & 1) == 0) {
                    draw_mesh(rr->mesh, p->x, p->z, rr->scale, rr->yaw);
                }
            } else if (rr->kind == RK_PLAYER_SHOT) {
                draw_box_rot(p->x, p->z + 0.18f, rr->scale * 0.34f, rr->scale * 2.2f, rr->scale * 0.55f, rr->yaw, rr->r, rr->g, rr->b, rr->a);
            } else if (rr->kind == RK_ENEMY_SHOT) {
                draw_box_rot(p->x, p->z, rr->scale * 0.95f, rr->scale * 0.95f, rr->scale * 0.62f, rr->yaw, rr->r, rr->g, rr->b, rr->a);
            } else if (rr->kind == RK_PARTICLE) {
                draw_box(p->x, p->z, rr->scale, rr->scale, rr->scale, rr->r, rr->g, rr->b, rr->a);
            } else if (rr->kind == RK_POWERUP) {
                float rot = g.time * 2.6f + rr->pulse;
                draw_box_rot(p->x, p->z, rr->scale * 1.05f, rr->scale * 1.05f, rr->scale * 0.42f, rot, rr->r, rr->g, rr->b, rr->a);
                draw_box_rot(p->x, p->z, rr->scale * 0.26f, rr->scale * 1.55f, rr->scale * 0.58f, -rot, 245, 250, 255, 230);
            }
        });

    if (cecs_is_alive(&g.world, g.player)) {
        Position *p = CECS_GET(&g.world, g.player, Position);
        Renderable *rr = CECS_GET(&g.world, g.player, Renderable);
        if (p && rr && rr->kind == RK_MESH) {
            sgl_load_pipeline(g.alpha_pip);
            draw_disc3(p->x, p->z, 1.45f, 38, 128, 190, 92);
            draw_ring3(p->x, p->z, 1.72f + 0.08f * sinf(g.time * 9.0f), 120, 220, 255, 210);
            draw_box(p->x - 0.48f, p->z - 0.8f, 0.16f, 0.62f, 0.08f, 72, 180, 255, 165);
            draw_box(p->x + 0.48f, p->z - 0.8f, 0.16f, 0.62f, 0.08f, 72, 180, 255, 165);
            draw_mesh(rr->mesh, p->x, p->z, rr->scale, rr->yaw);
        }
    }
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

static void draw_rect2(float x, float y, float w, float h, uint8_t r, uint8_t gg, uint8_t b, uint8_t a) {
    sgl_begin_triangles();
    sgl_v2f_c4b(x, y, r, gg, b, a);
    sgl_v2f_c4b(x + w, y, r, gg, b, a);
    sgl_v2f_c4b(x + w, y + h, r, gg, b, a);
    sgl_v2f_c4b(x, y, r, gg, b, a);
    sgl_v2f_c4b(x + w, y + h, r, gg, b, a);
    sgl_v2f_c4b(x, y + h, r, gg, b, a);
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
    sgl_viewportf(0.0f, 0.0f, w, h, true);
    sgl_scissor_rectf(0.0f, 0.0f, w, h, true);
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    if (g.flash > 0.0f) {
        uint8_t a = (uint8_t)clampf(g.flash * 120.0f, 0.0f, 120.0f);
        draw_rect2(0.0f, 0.0f, w, h, 255, 218, 160, a);
    }

    Health *hp = CECS_GET(&g.world, g.player, Health);

    float hud_y = gameplay_view_h();
    float hud_h = h - hud_y;
    if (hud_h < 0.0f) hud_h = 0.0f;
    draw_rect2(0.0f, hud_y, w, hud_h, 5, 10, 22, 230);
    draw_rect2(0.0f, hud_y, w, 2.0f, 82, 190, 255, 118);
    if (g.input.drag_down && g.input.drag_y >= hud_y) {
        float thumb_r = clampf(hud_h * 0.25f, 28.0f, 44.0f);
        float tx = clampf(g.input.drag_x, thumb_r, w - thumb_r);
        float ty = clampf(g.input.drag_y, hud_y + thumb_r, h - thumb_r);
        draw_disc2(tx, ty, thumb_r * 1.4f, 26, 80, 122, 95);
        draw_ring2(tx, ty, thumb_r * 1.4f, 104, 210, 255, 160);
    }

    float hud_pad = clampf(hud_h * 0.13f, 10.0f, 20.0f);
    float bar_x = hud_pad;
    float bar_w = w - hud_pad * 2.0f;
    float bar_y = hud_y + hud_h - hud_pad - 10.0f;
    draw_rect2(bar_x, bar_y, bar_w, 8.0f, 25, 39, 60, 190);
    float armor_t = hp ? clampf((float)hp->hp / 5.0f, 0.0f, 1.0f) : 0.0f;
    draw_rect2(bar_x, bar_y, bar_w * armor_t, 8.0f, 92, 230, 150, 230);
    for (int i = 0; i < 4; i++) {
        uint8_t a = (i < g.weapon_level) ? 225 : 70;
        draw_rect2(bar_x + (float)i * 24.0f, bar_y - 13.0f, 17.0f, 6.0f, 86, 210, 255, a);
    }

    float text_scale = clampf(sapp_dpi_scale(), 1.0f, 2.0f);
    float tw = w / text_scale;
    float th = h / text_scale;
    sdtx_canvas(tw, th);
    sdtx_font(0);
    sdtx_origin(hud_pad / text_scale, (hud_y + hud_pad) / text_scale);
    sdtx_color3b(210, 232, 255);
    sdtx_pos(0.0f, 0.0f);
    sdtx_printf("SCORE %06d", g.score);
    sdtx_pos(0.0f, 1.3f);
    int display_lives = g.lives < 0 ? 0 : g.lives;
    sdtx_printf("LIVES %d  ARMOR %d  WEAPON %d", display_lives, hp ? hp->hp : 0, g.weapon_level);
    if (g.game_over) {
        sdtx_origin(tw * 0.5f - 86.0f, th * 0.48f);
        sdtx_color3b(255, 210, 150);
        sdtx_pos(0.0f, 0.0f);
        sdtx_puts("TRAD STRIKE");
        sdtx_pos(0.0f, 1.5f);
        sdtx_color3b(165, 210, 255);
        sdtx_puts("TAP TO START");
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
        if (g.shake > 0.0f) g.shake = clampf(g.shake - g.dt * 1.9f, 0.0f, 1.0f);
        if (g.flash > 0.0f) g.flash = clampf(g.flash - g.dt * 1.7f, 0.0f, 1.0f);
        (void)cecs_schedule_run(&g.schedule, &g.world);
    } else {
        g.scroll += g.dt * 1.6f;
        if (g.shake > 0.0f) g.shake = clampf(g.shake - g.dt * 1.6f, 0.0f, 1.0f);
        if (g.flash > 0.0f) g.flash = clampf(g.flash - g.dt * 1.4f, 0.0f, 1.0f);
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

static void begin_drag(bool is_touch, uintptr_t touch_id, float x, float y) {
    g.input.drag_down = true;
    g.input.drag_is_touch = is_touch;
    g.input.drag_touch_id = touch_id;
    g.input.drag_x = x;
    g.input.drag_y = y;
    g.input.fire_pressed = true;
}

static void update_drag(float x, float y) {
    g.input.drag_x = x;
    g.input.drag_y = y;
}

static void end_drag(void) {
    g.input.drag_down = false;
    g.input.drag_is_touch = false;
    g.input.drag_touch_id = 0;
}

static int find_touch_index(const sapp_event *ev, uintptr_t id) {
    for (int i = 0; i < ev->num_touches; i++) {
        if (ev->touches[i].identifier == id) return i;
    }
    return -1;
}

static void touch_begin_or_move(const sapp_event *ev) {
    if (g.input.drag_down && g.input.drag_is_touch) {
        int index = find_touch_index(ev, g.input.drag_touch_id);
        if (index >= 0) {
            update_drag(ev->touches[index].pos_x, ev->touches[index].pos_y);
        }
        return;
    }
    if (!g.input.drag_down && ev->num_touches > 0) {
        const sapp_touchpoint *t = &ev->touches[0];
        begin_drag(true, t->identifier, t->pos_x, t->pos_y);
    }
}

static void touch_end_or_cancel(const sapp_event *ev) {
    if (g.input.drag_down && g.input.drag_is_touch) {
        int index = find_touch_index(ev, g.input.drag_touch_id);
        if (index >= 0) {
            update_drag(ev->touches[index].pos_x, ev->touches[index].pos_y);
        } else {
            end_drag();
        }
    }
}

static void event(const sapp_event *ev) {
    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (!ev->key_repeat && ev->key_code == SAPP_KEYCODE_R) {
            reset_game();
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && !g.input.drag_down) {
            begin_drag(false, 0, ev->mouse_x, ev->mouse_y);
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && g.input.drag_down && !g.input.drag_is_touch) {
            end_drag();
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (g.input.drag_down && !g.input.drag_is_touch) {
            update_drag(ev->mouse_x, ev->mouse_y);
        }
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
