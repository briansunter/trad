#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CECS_IMPLEMENTATION
#include "game.h"

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

Game g;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

uint32_t rnd_u32(void) {
    uint32_t x = g.rng ? g.rng : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g.rng = x;
    return x;
}

float rnd_range(float lo, float hi) {
    return lo + (hi - lo) * ((float)(rnd_u32() & 0xffffu) / 65535.0f);
}

float dist2(float ax, float az, float bx, float bz) {
    float dx = ax - bx;
    float dz = az - bz;
    return dx * dx + dz * dz;
}

float bottom_hud_h(void) {
    float h = g.viewport_h;
    float w = g.viewport_w;
    return (w < h * 1.18f) ? clampf(h * 0.17f, 118.0f, 168.0f)
                            : clampf(h * 0.10f, 72.0f, 110.0f);
}

float gameplay_view_h(void) {
    float h = g.viewport_h;
    if (h <= 240.0f) return h;
    float view_h = h - bottom_hud_h();
    return view_h < 240.0f ? 240.0f : view_h;
}

float world_half_x(void) {
    float aspect = g.viewport_w / (gameplay_view_h() > 1.0f ? gameplay_view_h() : 1.0f);
    float half_x = (WORLD_H + 1.5f) * aspect * 0.5f;
    float min_half = WORLD_W * 0.58f;
    return half_x < min_half ? min_half : half_x;
}

float player_muzzle_forward_world(float shot_scale) {
    float mesh_radius = trad_meshes[TRAD_MESH_CRAFT_RACER].radius * 1.45f;
    float projectile_tail = shot_scale * 1.15f;
    return mesh_radius + projectile_tail + 0.2f;
}

void add_component_types(cecs_world *world) {
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

cecs_entity spawn_mesh(uint8_t mesh, float x, float z, float scale, float yaw) {
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

void spawn_burst(float x, float z, int count, uint8_t r, uint8_t gg, uint8_t b) {
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

void spawn_pickup(float x, float z, uint8_t kind) {
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

void spawn_player(void) {
    g.player = spawn_mesh(TRAD_MESH_CRAFT_RACER, 0.0f, PLAYER_Z, 1.45f, 0.0f);
    Health h = { 3 };
    (void)CECS_ADD(&g.world, g.player, Health, &h);
    (void)CECS_ADD(&g.world, g.player, PlayerTag, NULL);
}

void spawn_player_shot(float x, float z, float vx, float vz, float scale, int damage, uint8_t r, uint8_t gg, uint8_t b) {
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

void spawn_player_shot_aligned(const Position *origin, float ship_heading, float side, float forward,
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

void spawn_enemy_shot_dir(float x, float z, float dx, float dz, float speed, float scale) {
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

void spawn_enemy_shot(float x, float z, float tx, float tz, float speed, float scale) {
    spawn_enemy_shot_dir(x, z, tx - x, tz - z, speed, scale);
}

void spawn_enemy_spread(float x, float z, float tx, float tz, int count, float spread, float speed) {
    float dx = tx - x;
    float dz = tz - z;
    float base = atan2f(dx, dz);
    float center = (float)(count - 1) * 0.5f;
    for (int i = 0; i < count; i++) {
        float a = base + ((float)i - center) * spread;
        spawn_enemy_shot_dir(x, z, sinf(a), cosf(a), speed, 0.27f);
    }
}

void spawn_enemy(float x, float z, int type) {
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

void spawn_wave(void) {
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

void reset_game(void) {
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

void drag_target_world(float *out_x, float *out_z) {
    float w = g.viewport_w;
    float h = gameplay_view_h();
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;
    float half_x = world_half_x();
    float ship_offset = clampf(g.viewport_h * 0.06f, 60.0f, 160.0f);
    float sx = clampf(g.input.drag_x, 0.0f, w);
    float sy = clampf(g.input.drag_y - ship_offset, 0.0f, h);
    *out_x = clampf(half_x - (sx / w) * half_x * 2.0f, -half_x + 0.5f, half_x - 0.5f);
    *out_z = clampf((WORLD_H + 1.25f) - (sy / h) * (WORLD_H + 2.5f), PLAYER_MIN_Z, PLAYER_MAX_Z);
}

void player_system(cecs_world *world, void *ctx) {
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

void enemy_system(cecs_world *world, void *ctx) {
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

void spawn_system(cecs_world *world, void *ctx) {
    (void)world;
    (void)ctx;
    g.spawn_timer -= g.dt;
    if (g.spawn_timer <= 0.0f) {
        spawn_wave();
        g.spawn_timer = clampf(1.05f - g.time * 0.0065f, 0.38f, 1.05f);
    }
}

void movement_system(cecs_world *world, void *ctx) {
    (void)ctx;
    CECS_QUERY_EACH(world,
        ((cecs_component_id[]){ CECS_COMPONENT_ID(Position), CECS_COMPONENT_ID(Velocity) }), {
            Position *p = CECS_QUERY_GET_FAST(&_q, Position);
            Velocity *v = CECS_QUERY_GET_FAST(&_q, Velocity);
            p->x += v->x * g.dt;
            p->z += v->z * g.dt;
        });
}

void lifetime_system(cecs_world *world, void *ctx) {
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

void cleanup_system(cecs_world *world, void *ctx) {
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

void hit_player(cecs_world *world, float x, float z) {
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

void collision_system(cecs_world *world, void *ctx) {
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

void init_stars(void) {
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

void begin_drag(bool is_touch, uintptr_t touch_id, float x, float y) {
    g.input.drag_down = true;
    g.input.drag_is_touch = is_touch;
    g.input.drag_touch_id = touch_id;
    g.input.drag_x = x;
    g.input.drag_y = y;
    g.input.fire_pressed = true;
}

void update_drag(float x, float y) {
    g.input.drag_x = x;
    g.input.drag_y = y;
}

void end_drag(void) {
    g.input.drag_down = false;
    g.input.drag_is_touch = false;
    g.input.drag_touch_id = 0;
}

void game_init(void) {
    g.rng = 0x9146b72du;
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

void game_tick(float dt, float viewport_w, float viewport_h) {
    g.viewport_w = viewport_w;
    g.viewport_h = viewport_h;
    g.dt = clampf(dt, 1.0f / 120.0f, 1.0f / 30.0f);
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
}
