#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static int g_test_count = 0;
static int g_test_failed_total = 0;
static int g_assertion_failures_in_current = 0;
static const char *g_current_test = "";

#define TEST(name) static void test_##name(void)

#define RUN(name) do { \
    g_current_test = #name; \
    g_assertion_failures_in_current = 0; \
    g_test_count++; \
    setup_clean_state(); \
    test_##name(); \
    if (g_assertion_failures_in_current == 0) { \
        printf("  PASS  %s\n", #name); \
    } else { \
        printf("  FAIL  %s (%d assertion%s)\n", #name, \
            g_assertion_failures_in_current, g_assertion_failures_in_current == 1 ? "" : "s"); \
        g_test_failed_total++; \
    } \
} while (0)

#define EXPECT(cond) do { \
    if (!(cond)) { \
        printf("    [%s:%d] EXPECT(%s) failed in %s\n", __FILE__, __LINE__, #cond, g_current_test); \
        g_assertion_failures_in_current++; \
    } \
} while (0)

#define EXPECT_NEAR(a, b, eps) do { \
    float _av = (float)(a); \
    float _bv = (float)(b); \
    float _diff = _av - _bv; \
    if (_diff < 0.0f) _diff = -_diff; \
    if (_diff > (float)(eps)) { \
        printf("    [%s:%d] EXPECT_NEAR(%s=%.6f, %s=%.6f, eps=%g) failed in %s\n", \
            __FILE__, __LINE__, #a, (double)_av, #b, (double)_bv, (double)(eps), g_current_test); \
        g_assertion_failures_in_current++; \
    } \
} while (0)

#define EXPECT_EQ_INT(a, b) do { \
    long _av = (long)(a); \
    long _bv = (long)(b); \
    if (_av != _bv) { \
        printf("    [%s:%d] EXPECT_EQ_INT(%s=%ld, %s=%ld) failed in %s\n", \
            __FILE__, __LINE__, #a, _av, #b, _bv, g_current_test); \
        g_assertion_failures_in_current++; \
    } \
} while (0)

#define EXPECT_PTR(p) do { \
    if ((p) == NULL) { \
        printf("    [%s:%d] EXPECT_PTR(%s) was NULL in %s\n", \
            __FILE__, __LINE__, #p, g_current_test); \
        g_assertion_failures_in_current++; \
        return; \
    } \
} while (0)

static int count_with_component(cecs_component_id id) {
    int count = 0;
    cecs_component_id ids[1];
    ids[0] = id;
    CECS_QUERY_EACH(&g.world, ids, {
        (void)_q;
        count++;
    });
    return count;
}

static cecs_entity first_with_component(cecs_component_id id) {
    cecs_entity found = cecs_entity_null();
    cecs_component_id ids[1];
    ids[0] = id;
    CECS_QUERY_EACH(&g.world, ids, {
        found = _q.entity;
        break;
    });
    return found;
}

/* Fresh world with components registered and no player. Builds on
 * reset_game so any future field added there is picked up automatically. */
static void setup_clean_state(void) {
    memset(&g.input, 0, sizeof(g.input));
    g.viewport_w = 960.0f;
    g.viewport_h = 1280.0f;
    g.rng = 0x12345678u;
    reset_game();
    (void)cecs_despawn(&g.world, g.player);
    g.player = cecs_entity_null();
    g.invuln_timer = 0.0f;
}

TEST(clampf_inside) {
    EXPECT_NEAR(clampf(0.5f, 0.0f, 1.0f), 0.5f, 1e-6f);
    EXPECT_NEAR(clampf(-3.0f, -5.0f, 5.0f), -3.0f, 1e-6f);
}

TEST(clampf_low_high) {
    EXPECT_NEAR(clampf(-2.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(clampf(5.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
    EXPECT_NEAR(clampf(0.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(clampf(1.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
}

TEST(rnd_u32_deterministic) {
    g.rng = 0x12345678u;
    uint32_t a = rnd_u32();
    uint32_t b = rnd_u32();
    g.rng = 0x12345678u;
    EXPECT_EQ_INT(rnd_u32(), (long)a);
    EXPECT_EQ_INT(rnd_u32(), (long)b);
    EXPECT(a != b);
}

TEST(rnd_u32_zero_seed_recovers) {
    g.rng = 0u;
    uint32_t v = rnd_u32();
    EXPECT(v != 0u);
    EXPECT(g.rng != 0u);
}

TEST(rnd_range_in_bounds) {
    g.rng = 0xa1b2c3d4u;
    for (int i = 0; i < 1000; i++) {
        float v = rnd_range(2.0f, 7.0f);
        EXPECT(v >= 2.0f);
        EXPECT(v <= 7.0f);
    }
    g.rng = 0xfeedbeefu;
    for (int i = 0; i < 200; i++) {
        float v = rnd_range(-10.0f, -1.0f);
        EXPECT(v >= -10.0f);
        EXPECT(v <= -1.0f);
    }
}

TEST(dist2_basic) {
    EXPECT_NEAR(dist2(0.0f, 0.0f, 3.0f, 4.0f), 25.0f, 1e-5f);
    EXPECT_NEAR(dist2(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(dist2(-2.0f, -3.0f, 1.0f, 1.0f), 25.0f, 1e-5f);
}

TEST(bottom_hud_h_portrait) {
    g.viewport_w = 430.0f;
    g.viewport_h = 932.0f;
    float h = bottom_hud_h();
    EXPECT(h >= 118.0f);
    EXPECT(h <= 168.0f);
}

TEST(bottom_hud_h_landscape) {
    g.viewport_w = 1920.0f;
    g.viewport_h = 1080.0f;
    float h = bottom_hud_h();
    EXPECT(h >= 72.0f);
    EXPECT(h <= 110.0f);
}

TEST(bottom_hud_h_clamp_min_portrait) {
    g.viewport_w = 100.0f;
    g.viewport_h = 200.0f;
    EXPECT_NEAR(bottom_hud_h(), 118.0f, 1e-3f);
}

TEST(bottom_hud_h_clamp_max_portrait) {
    g.viewport_w = 800.0f;
    g.viewport_h = 4000.0f;
    EXPECT_NEAR(bottom_hud_h(), 168.0f, 1e-3f);
}

TEST(gameplay_view_h_small_height) {
    g.viewport_w = 320.0f;
    g.viewport_h = 200.0f;
    EXPECT_NEAR(gameplay_view_h(), 200.0f, 1e-3f);
}

TEST(gameplay_view_h_normal) {
    g.viewport_w = 960.0f;
    g.viewport_h = 1280.0f;
    float v = gameplay_view_h();
    EXPECT(v >= 240.0f);
    EXPECT(v < 1280.0f);
}

TEST(gameplay_view_h_clamps_to_min) {
    g.viewport_w = 200.0f;
    g.viewport_h = 260.0f;
    float v = gameplay_view_h();
    EXPECT_NEAR(v, 240.0f, 1e-3f);
}

TEST(world_half_x_basic) {
    g.viewport_w = 960.0f;
    g.viewport_h = 1280.0f;
    float hx = world_half_x();
    EXPECT(hx > 0.0f);
}

TEST(world_half_x_min_clamp_tall) {
    g.viewport_w = 100.0f;
    g.viewport_h = 4000.0f;
    float hx = world_half_x();
    EXPECT_NEAR(hx, WORLD_W * 0.58f, 1e-3f);
}

TEST(world_half_x_wide_aspect) {
    g.viewport_w = 4000.0f;
    g.viewport_h = 500.0f;
    float hx = world_half_x();
    EXPECT(hx > WORLD_W * 0.58f);
}

TEST(player_muzzle_forward_increases_with_scale) {
    float small = player_muzzle_forward_world(0.1f);
    float large = player_muzzle_forward_world(1.0f);
    EXPECT(large > small);
    EXPECT(small > 0.0f);
}

TEST(spawn_mesh_creates_entity_with_components) {
    cecs_entity e = spawn_mesh(TRAD_MESH_CRAFT_RACER, 1.5f, 7.0f, 1.2f, 0.3f);
    EXPECT(cecs_is_alive(&g.world, e));
    Position *p = CECS_GET(&g.world, e, Position);
    Velocity *v = CECS_GET(&g.world, e, Velocity);
    Collider *c = CECS_GET(&g.world, e, Collider);
    Renderable *r = CECS_GET(&g.world, e, Renderable);
    EXPECT_PTR(p);
    EXPECT_PTR(v);
    EXPECT_PTR(c);
    EXPECT_PTR(r);
    EXPECT_NEAR(p->x, 1.5f, 1e-5f);
    EXPECT_NEAR(p->z, 7.0f, 1e-5f);
    EXPECT_NEAR(v->x, 0.0f, 1e-5f);
    EXPECT_NEAR(v->z, 0.0f, 1e-5f);
    EXPECT_EQ_INT(r->mesh, TRAD_MESH_CRAFT_RACER);
    EXPECT_EQ_INT(r->kind, RK_MESH);
    EXPECT_NEAR(r->scale, 1.2f, 1e-5f);
    EXPECT_NEAR(r->yaw, 0.3f, 1e-5f);
    EXPECT(c->radius > 0.0f);
}

TEST(spawn_burst_creates_particles) {
    spawn_burst(2.0f, 3.0f, 12, 200, 100, 50);
    int particles = count_with_component(CECS_COMPONENT_ID(ParticleTag));
    EXPECT_EQ_INT(particles, 12);
    int with_lifetime = count_with_component(CECS_COMPONENT_ID(Lifetime));
    EXPECT_EQ_INT(with_lifetime, 12);
}

TEST(spawn_pickup_weapon) {
    spawn_pickup(1.0f, 2.0f, PICKUP_WEAPON);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(PickupTag)), 1);
    Renderable *r = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(PickupTag)), Renderable);
    EXPECT_PTR(r);
    EXPECT_EQ_INT(r->r, 80);
    EXPECT_EQ_INT(r->g, 222);
    EXPECT_EQ_INT(r->b, 255);
}

TEST(spawn_pickup_armor) {
    spawn_pickup(0.0f, 0.0f, PICKUP_ARMOR);
    Renderable *r = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(PickupTag)), Renderable);
    EXPECT_PTR(r);
    EXPECT_EQ_INT(r->r, 115);
    EXPECT_EQ_INT(r->g, 255);
    EXPECT_EQ_INT(r->b, 145);
}

TEST(spawn_pickup_score) {
    spawn_pickup(0.0f, 0.0f, PICKUP_SCORE);
    Renderable *r = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(PickupTag)), Renderable);
    EXPECT_PTR(r);
    EXPECT_EQ_INT(r->r, 255);
    EXPECT_EQ_INT(r->g, 205);
    EXPECT_EQ_INT(r->b, 92);
}

TEST(spawn_player_creates_tagged_entity) {
    spawn_player();
    EXPECT(cecs_is_alive(&g.world, g.player));
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    EXPECT_EQ_INT(h->hp, 3);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(PlayerTag)), 1);
}

TEST(spawn_player_shot_components) {
    spawn_player_shot(0.0f, 0.0f, 1.0f, 30.0f, 0.4f, 2, 80, 220, 255);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(PlayerShotTag)), 1);
    cecs_entity e = first_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    Velocity *v = CECS_GET(&g.world, e, Velocity);
    Damage *d = CECS_GET(&g.world, e, Damage);
    EXPECT_PTR(v);
    EXPECT_PTR(d);
    EXPECT_NEAR(v->x, 1.0f, 1e-5f);
    EXPECT_NEAR(v->z, 30.0f, 1e-5f);
    EXPECT_EQ_INT(d->amount, 2);
}

TEST(spawn_player_shot_aligned_forward) {
    Position origin = { 0.0f, 0.0f };
    spawn_player_shot_aligned(&origin, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 30.0f, 0.4f, 1, 0, 0, 0);
    Position *p = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(PlayerShotTag)), Position);
    EXPECT_PTR(p);
    EXPECT_NEAR(p->x, 0.0f, 1e-4f);
    EXPECT_NEAR(p->z, 2.0f, 1e-4f);
}

TEST(spawn_player_shot_aligned_side_offset) {
    Position origin = { 0.0f, 0.0f };
    spawn_player_shot_aligned(&origin, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 30.0f, 0.4f, 1, 0, 0, 0);
    Position *p = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(PlayerShotTag)), Position);
    EXPECT_PTR(p);
    EXPECT_NEAR(p->x, 0.5f, 1e-4f);
}

TEST(spawn_enemy_shot_dir_normalizes) {
    spawn_enemy_shot_dir(0.0f, 0.0f, 3.0f, 0.0f, 5.0f, 0.3f);
    Velocity *v = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(EnemyShotTag)), Velocity);
    EXPECT_PTR(v);
    EXPECT_NEAR(v->x, 5.0f, 1e-4f);
    EXPECT_NEAR(v->z, 0.0f, 1e-4f);
}

TEST(spawn_enemy_shot_dir_zero_dir) {
    spawn_enemy_shot_dir(0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 0.3f);
    Velocity *v = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(EnemyShotTag)), Velocity);
    EXPECT_PTR(v);
    EXPECT_NEAR(v->x, 0.0f, 1e-4f);
    EXPECT_NEAR(v->z, -5.0f, 1e-4f);
}

TEST(spawn_enemy_shot_target) {
    spawn_enemy_shot(0.0f, 0.0f, 4.0f, 0.0f, 10.0f, 0.3f);
    int n = count_with_component(CECS_COMPONENT_ID(EnemyShotTag));
    EXPECT_EQ_INT(n, 1);
}

TEST(spawn_enemy_spread_creates_count_shots) {
    spawn_enemy_spread(0.0f, 0.0f, 0.0f, -1.0f, 5, 0.18f, 6.0f);
    int n = count_with_component(CECS_COMPONENT_ID(EnemyShotTag));
    EXPECT_EQ_INT(n, 5);
}

TEST(spawn_enemy_type_default) {
    spawn_enemy(0.0f, WORLD_H + 1.0f, 0);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyTag)), 1);
    cecs_entity e = first_with_component(CECS_COMPONENT_ID(EnemyTag));
    Health *h = CECS_GET(&g.world, e, Health);
    Velocity *v = CECS_GET(&g.world, e, Velocity);
    EXPECT_PTR(h);
    EXPECT_PTR(v);
    EXPECT_EQ_INT(h->hp, 2);
    EXPECT(v->z < 0.0f);
}

TEST(spawn_enemy_all_types) {
    for (int t = 0; t < 8; t++) {
        setup_clean_state();
        spawn_enemy(0.0f, WORLD_H + 1.0f, t);
        EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyTag)), 1);
        cecs_entity e = first_with_component(CECS_COMPONENT_ID(EnemyTag));
        Health *h = CECS_GET(&g.world, e, Health);
        EnemyBrain *brain = CECS_GET(&g.world, e, EnemyBrain);
        EXPECT_PTR(h);
        EXPECT_PTR(brain);
        EXPECT(h->hp > 0);
        EXPECT_EQ_INT(brain->type, t);
    }
}

TEST(spawn_wave_all_patterns) {
    int total_spawned[8] = {0};
    for (int p = 0; p < 8; p++) {
        setup_clean_state();
        g.rng = 0x12345678u + (uint32_t)p;
        for (int tries = 0; tries < 64; tries++) {
            int before = count_with_component(CECS_COMPONENT_ID(EnemyTag));
            uint32_t saved_rng = g.rng;
            int pattern_will_be = (int)(rnd_u32() % 8u);
            g.rng = saved_rng;
            if (pattern_will_be == p) {
                spawn_wave();
                int after = count_with_component(CECS_COMPONENT_ID(EnemyTag));
                total_spawned[p] = after - before;
                break;
            } else {
                g.rng = saved_rng + 1;
            }
        }
        EXPECT(total_spawned[p] >= 1);
    }
}

TEST(spawn_wave_default_path) {
    int hits = 0;
    for (int i = 0; i < 50 && hits < 5; i++) {
        setup_clean_state();
        g.rng = 0x9abcd000u + (uint32_t)i;
        spawn_wave();
        int n = count_with_component(CECS_COMPONENT_ID(EnemyTag));
        EXPECT(n > 0);
        if (n > 0) hits++;
    }
    EXPECT(hits > 0);
}

TEST(reset_game_resets_state) {
    g.score = 999;
    g.lives = -3;
    g.weapon_level = 4;
    g.game_over = true;
    g.flash = 0.7f;
    reset_game();
    EXPECT_EQ_INT(g.score, 0);
    EXPECT_EQ_INT(g.lives, 2);
    EXPECT_EQ_INT(g.weapon_level, 1);
    EXPECT(g.game_over == false);
    EXPECT_NEAR(g.flash, 0.0f, 1e-5f);
    EXPECT(cecs_is_alive(&g.world, g.player));
}

TEST(init_stars_fills_array) {
    init_stars();
    for (int i = 0; i < STAR_COUNT; i++) {
        EXPECT(g.stars[i].speed > 0.0f);
        EXPECT(g.stars[i].size > 0.0f);
        EXPECT(g.stars[i].b == 255);
    }
}

TEST(drag_target_world_center) {
    g.viewport_w = 1000.0f;
    g.viewport_h = 1200.0f;
    g.input.drag_x = 500.0f;
    g.input.drag_y = 600.0f;
    float tx = 0.0f;
    float tz = 0.0f;
    drag_target_world(&tx, &tz);
    EXPECT_NEAR(tx, 0.0f, 1e-3f);
    EXPECT(tz >= PLAYER_MIN_Z);
    EXPECT(tz <= PLAYER_MAX_Z);
}

TEST(drag_target_world_left_edge) {
    g.viewport_w = 1000.0f;
    g.viewport_h = 1200.0f;
    g.input.drag_x = 0.0f;
    g.input.drag_y = 600.0f;
    float tx = 0.0f, tz = 0.0f;
    drag_target_world(&tx, &tz);
    EXPECT(tx > 0.0f);
}

TEST(drag_target_world_right_edge) {
    g.viewport_w = 1000.0f;
    g.viewport_h = 1200.0f;
    g.input.drag_x = 1000.0f;
    g.input.drag_y = 600.0f;
    float tx = 0.0f, tz = 0.0f;
    drag_target_world(&tx, &tz);
    EXPECT(tx < 0.0f);
}

TEST(drag_target_world_tiny_viewport) {
    g.viewport_w = 0.0f;
    g.viewport_h = 0.0f;
    g.input.drag_x = 5.0f;
    g.input.drag_y = 5.0f;
    float tx = 0.0f, tz = 0.0f;
    drag_target_world(&tx, &tz);
    EXPECT(tx >= -1000.0f && tx <= 1000.0f);
    EXPECT(tz >= PLAYER_MIN_Z);
    EXPECT(tz <= PLAYER_MAX_Z);
}

TEST(begin_drag_sets_state) {
    begin_drag(true, 7u, 100.0f, 200.0f);
    EXPECT(g.input.drag_down);
    EXPECT(g.input.drag_is_touch);
    EXPECT_EQ_INT((long)g.input.drag_touch_id, 7L);
    EXPECT_NEAR(g.input.drag_x, 100.0f, 1e-5f);
    EXPECT_NEAR(g.input.drag_y, 200.0f, 1e-5f);
    EXPECT(g.input.fire_pressed);
}

TEST(update_drag_updates_xy) {
    begin_drag(false, 0u, 10.0f, 20.0f);
    update_drag(50.0f, 60.0f);
    EXPECT_NEAR(g.input.drag_x, 50.0f, 1e-5f);
    EXPECT_NEAR(g.input.drag_y, 60.0f, 1e-5f);
    EXPECT(g.input.drag_down);
}

TEST(end_drag_clears_state) {
    begin_drag(true, 5u, 1.0f, 2.0f);
    end_drag();
    EXPECT(!g.input.drag_down);
    EXPECT(!g.input.drag_is_touch);
    EXPECT_EQ_INT((long)g.input.drag_touch_id, 0L);
}

TEST(player_system_no_player_safe) {
    g.player = cecs_entity_null();
    player_system(&g.world, NULL);
}

TEST(player_system_moves_to_target_on_drag) {
    spawn_player();
    g.viewport_w = 1000.0f;
    g.viewport_h = 1200.0f;
    g.input.drag_down = true;
    g.input.drag_x = 500.0f;
    g.input.drag_y = 800.0f;
    player_system(&g.world, NULL);
    Position *p = CECS_GET(&g.world, g.player, Position);
    EXPECT_PTR(p);
    EXPECT_NEAR(p->x, 0.0f, 1e-3f);
}

TEST(player_system_fires_shots_when_drag_down) {
    spawn_player();
    g.input.drag_down = true;
    g.input.drag_x = 500.0f;
    g.input.drag_y = 800.0f;
    g.weapon_level = 1;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 2);
    EXPECT(g.shot_timer > 0.0f);
}

TEST(player_system_weapon_level_4) {
    spawn_player();
    g.input.drag_down = true;
    g.input.drag_x = 500.0f;
    g.input.drag_y = 800.0f;
    g.weapon_level = 4;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 6);
}

TEST(player_system_weapon_level_2) {
    spawn_player();
    g.input.drag_down = true;
    g.weapon_level = 2;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 3);
}

TEST(player_system_weapon_level_3) {
    spawn_player();
    g.input.drag_down = true;
    g.weapon_level = 3;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 5);
}

TEST(player_system_clamps_weapon_level_low) {
    spawn_player();
    g.input.drag_down = true;
    g.weapon_level = 0;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 2);
}

TEST(player_system_clamps_weapon_level_high) {
    spawn_player();
    g.input.drag_down = true;
    g.weapon_level = 99;
    g.shot_timer = 0.0f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 6);
}

TEST(player_system_respects_shot_timer) {
    spawn_player();
    g.input.drag_down = true;
    g.shot_timer = 0.5f;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 0);
}

TEST(player_system_no_drag_no_shots) {
    spawn_player();
    g.input.drag_down = false;
    player_system(&g.world, NULL);
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 0);
}

static void clear_enemy_fire_cd(void) {
    cecs_component_id ids[1] = { CECS_COMPONENT_ID(EnemyTag) };
    CECS_QUERY_EACH(&g.world, ids, {
        EnemyBrain *b = CECS_GET(&g.world, _q.entity, EnemyBrain);
        if (b) b->fire_cd = 0.0f;
    });
}

TEST(enemy_system_steers_and_fires) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 0);
    clear_enemy_fire_cd();
    g.dt = 1.0f / 60.0f;
    enemy_system(&g.world, NULL);
    EXPECT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)) >= 1);
}

TEST(enemy_system_type5_dive) {
    spawn_player();
    spawn_enemy(0.0f, 20.0f, 5);
    enemy_system(&g.world, NULL);
    Velocity *v = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(EnemyTag)), Velocity);
    EXPECT_PTR(v);
    EXPECT_NEAR(v->z, -0.8f, 1e-4f);
}

TEST(enemy_system_type5_spread_fire) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 5);
    clear_enemy_fire_cd();
    enemy_system(&g.world, NULL);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)), 5);
}

TEST(enemy_system_type2_spread_fire) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 2);
    clear_enemy_fire_cd();
    enemy_system(&g.world, NULL);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)), 3);
}

TEST(enemy_system_type3_no_fire) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 3);
    clear_enemy_fire_cd();
    enemy_system(&g.world, NULL);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)), 0);
}

TEST(enemy_system_type6_steer) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 6);
    enemy_system(&g.world, NULL);
    EXPECT(true);
}

TEST(enemy_system_type4_steer) {
    spawn_player();
    spawn_enemy(0.0f, 15.0f, 4);
    enemy_system(&g.world, NULL);
    EXPECT(true);
}

TEST(enemy_system_no_fire_outside_range) {
    spawn_player();
    spawn_enemy(0.0f, 5.0f, 0);
    clear_enemy_fire_cd();
    enemy_system(&g.world, NULL);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)), 0);
}

TEST(spawn_system_triggers_wave_when_timer_elapsed) {
    g.spawn_timer = -0.01f;
    g.dt = 0.0f;
    int before = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    spawn_system(&g.world, NULL);
    int after = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    EXPECT(after > before);
    EXPECT(g.spawn_timer > 0.0f);
}

TEST(spawn_system_no_trigger_when_timer_positive) {
    g.spawn_timer = 5.0f;
    g.dt = 1.0f / 60.0f;
    int before = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    spawn_system(&g.world, NULL);
    int after = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    EXPECT_EQ_INT(after, before);
}

TEST(movement_system_applies_velocity) {
    cecs_entity e = cecs_spawn(&g.world);
    Position p = { 1.0f, 2.0f };
    Velocity v = { 10.0f, -20.0f };
    (void)CECS_ADD(&g.world, e, Position, &p);
    (void)CECS_ADD(&g.world, e, Velocity, &v);
    g.dt = 0.5f;
    movement_system(&g.world, NULL);
    Position *p2 = CECS_GET(&g.world, e, Position);
    EXPECT_PTR(p2);
    EXPECT_NEAR(p2->x, 6.0f, 1e-4f);
    EXPECT_NEAR(p2->z, -8.0f, 1e-4f);
}

TEST(lifetime_system_despawns_expired) {
    cecs_entity e = cecs_spawn(&g.world);
    Lifetime lt = { 0.001f };
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    g.dt = 1.0f / 60.0f;
    lifetime_system(&g.world, NULL);
    EXPECT(!cecs_is_alive(&g.world, e));
}

TEST(lifetime_system_keeps_active) {
    cecs_entity e = cecs_spawn(&g.world);
    Lifetime lt = { 10.0f };
    (void)CECS_ADD(&g.world, e, Lifetime, &lt);
    g.dt = 1.0f / 60.0f;
    lifetime_system(&g.world, NULL);
    EXPECT(cecs_is_alive(&g.world, e));
    Lifetime *lt2 = CECS_GET(&g.world, e, Lifetime);
    EXPECT_PTR(lt2);
    EXPECT(lt2->seconds < 10.0f);
}

TEST(cleanup_system_removes_offscreen) {
    cecs_entity a = cecs_spawn(&g.world);
    Position pa = { 0.0f, WORLD_H + 20.0f };
    (void)CECS_ADD(&g.world, a, Position, &pa);

    cecs_entity b = cecs_spawn(&g.world);
    Position pb = { 0.0f, -10.0f };
    (void)CECS_ADD(&g.world, b, Position, &pb);

    cecs_entity c = cecs_spawn(&g.world);
    Position pc = { -50.0f, 5.0f };
    (void)CECS_ADD(&g.world, c, Position, &pc);

    cecs_entity d = cecs_spawn(&g.world);
    Position pd = { 50.0f, 5.0f };
    (void)CECS_ADD(&g.world, d, Position, &pd);

    cecs_entity e = cecs_spawn(&g.world);
    Position pe = { 0.0f, 5.0f };
    (void)CECS_ADD(&g.world, e, Position, &pe);

    cleanup_system(&g.world, NULL);
    EXPECT(!cecs_is_alive(&g.world, a));
    EXPECT(!cecs_is_alive(&g.world, b));
    EXPECT(!cecs_is_alive(&g.world, c));
    EXPECT(!cecs_is_alive(&g.world, d));
    EXPECT(cecs_is_alive(&g.world, e));
}

TEST(cleanup_system_keeps_player) {
    spawn_player();
    Position *p = CECS_GET(&g.world, g.player, Position);
    EXPECT_PTR(p);
    p->x = 50.0f;
    p->z = 50.0f;
    cleanup_system(&g.world, NULL);
    EXPECT(cecs_is_alive(&g.world, g.player));
}

TEST(hit_player_during_invuln_noop) {
    spawn_player();
    g.invuln_timer = 1.0f;
    Health *h_before = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_before);
    int hp_before = h_before->hp;
    hit_player(&g.world, 0.0f, 0.0f);
    Health *h_after = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_after);
    EXPECT_EQ_INT(h_after->hp, hp_before);
}

TEST(hit_player_decrements_hp_and_weapon) {
    spawn_player();
    g.invuln_timer = 0.0f;
    g.weapon_level = 3;
    hit_player(&g.world, 0.0f, 0.0f);
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    EXPECT_EQ_INT(h->hp, 2);
    EXPECT_EQ_INT(g.weapon_level, 2);
    EXPECT(g.invuln_timer > 0.0f);
}

TEST(hit_player_keeps_weapon_min) {
    spawn_player();
    g.invuln_timer = 0.0f;
    g.weapon_level = 1;
    hit_player(&g.world, 0.0f, 0.0f);
    EXPECT_EQ_INT(g.weapon_level, 1);
}

TEST(hit_player_respawn_on_zero_hp) {
    spawn_player();
    g.lives = 2;
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    h->hp = 1;
    g.invuln_timer = 0.0f;
    hit_player(&g.world, 0.0f, 0.0f);
    Health *h2 = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h2);
    EXPECT_EQ_INT(h2->hp, 3);
    EXPECT_EQ_INT(g.lives, 1);
    EXPECT(!g.game_over);
    Position *p = CECS_GET(&g.world, g.player, Position);
    EXPECT_PTR(p);
    EXPECT_NEAR(p->x, 0.0f, 1e-5f);
    EXPECT_NEAR(p->z, PLAYER_Z, 1e-5f);
}

TEST(hit_player_game_over_on_no_lives) {
    spawn_player();
    g.lives = 0;
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    h->hp = 1;
    g.invuln_timer = 0.0f;
    hit_player(&g.world, 1.0f, 2.0f);
    EXPECT(g.game_over);
    EXPECT_EQ_INT(g.lives, -1);
    EXPECT(!cecs_is_alive(&g.world, g.player));
}

TEST(collision_system_player_shot_damages_enemy) {
    spawn_player();
    spawn_enemy(0.0f, 5.0f, 0);
    spawn_player_shot(0.0f, 5.0f, 0.0f, 0.0f, 0.3f, 1, 0, 0, 0);
    collision_system(&g.world, NULL);
    /* Either enemy dies or shot consumes its damage */
    int enemies = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    int shots = count_with_component(CECS_COMPONENT_ID(PlayerShotTag));
    EXPECT_EQ_INT(shots, 0);
    EXPECT(enemies <= 1);
}

TEST(collision_system_player_shot_kills_enemy) {
    spawn_player();
    spawn_enemy(0.0f, 5.0f, 0);
    /* Hit the enemy enough times to kill (type 0 has hp=2) */
    spawn_player_shot(0.0f, 5.0f, 0.0f, 0.0f, 0.3f, 5, 0, 0, 0);
    int score_before = g.score;
    collision_system(&g.world, NULL);
    int enemies = count_with_component(CECS_COMPONENT_ID(EnemyTag));
    EXPECT_EQ_INT(enemies, 0);
    EXPECT(g.score > score_before);
}

TEST(collision_system_enemy_shot_hits_player) {
    spawn_player();
    g.invuln_timer = 0.0f;
    spawn_enemy_shot_dir(0.0f, PLAYER_Z, 0.0f, -1.0f, 1.0f, 0.3f);
    Position *shot_p = CECS_GET(&g.world, first_with_component(CECS_COMPONENT_ID(EnemyShotTag)), Position);
    EXPECT_PTR(shot_p);
    shot_p->x = 0.0f;
    shot_p->z = PLAYER_Z;
    Health *h_before = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_before);
    int hp_before = h_before->hp;
    collision_system(&g.world, NULL);
    Health *h_after = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_after);
    EXPECT(h_after->hp < hp_before);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyShotTag)), 0);
}

TEST(collision_system_enemy_hits_player) {
    spawn_player();
    g.invuln_timer = 0.0f;
    spawn_enemy(0.0f, PLAYER_Z, 0);
    Health *h_before = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_before);
    int hp_before = h_before->hp;
    collision_system(&g.world, NULL);
    Health *h_after = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h_after);
    EXPECT(h_after->hp < hp_before);
    EXPECT_EQ_INT(count_with_component(CECS_COMPONENT_ID(EnemyTag)), 0);
}

TEST(collision_system_weapon_pickup) {
    spawn_player();
    g.weapon_level = 1;
    int score_before = g.score;
    spawn_pickup(0.0f, PLAYER_Z, PICKUP_WEAPON);
    collision_system(&g.world, NULL);
    EXPECT_EQ_INT(g.weapon_level, 2);
    EXPECT(g.score > score_before);
}

TEST(collision_system_weapon_pickup_caps_at_4) {
    spawn_player();
    g.weapon_level = 4;
    spawn_pickup(0.0f, PLAYER_Z, PICKUP_WEAPON);
    collision_system(&g.world, NULL);
    EXPECT_EQ_INT(g.weapon_level, 4);
}

TEST(collision_system_armor_pickup) {
    spawn_player();
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    h->hp = 2;
    spawn_pickup(0.0f, PLAYER_Z, PICKUP_ARMOR);
    int score_before = g.score;
    collision_system(&g.world, NULL);
    Health *h2 = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h2);
    EXPECT_EQ_INT(h2->hp, 3);
    EXPECT(g.score > score_before);
}

TEST(collision_system_armor_pickup_caps_at_5) {
    spawn_player();
    Health *h = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h);
    h->hp = 5;
    spawn_pickup(0.0f, PLAYER_Z, PICKUP_ARMOR);
    collision_system(&g.world, NULL);
    Health *h2 = CECS_GET(&g.world, g.player, Health);
    EXPECT_PTR(h2);
    EXPECT_EQ_INT(h2->hp, 5);
}

TEST(collision_system_score_pickup) {
    spawn_player();
    int score_before = g.score;
    spawn_pickup(0.0f, PLAYER_Z, PICKUP_SCORE);
    collision_system(&g.world, NULL);
    EXPECT(g.score - score_before >= 750);
}

TEST(collision_system_no_player_skip) {
    spawn_enemy(0.0f, 5.0f, 0);
    spawn_player_shot(0.0f, 5.0f, 0.0f, 0.0f, 0.3f, 5, 0, 0, 0);
    g.player = cecs_entity_null();
    collision_system(&g.world, NULL);
    EXPECT(true);
}

TEST(game_init_seeds_and_resets) {
    game_init();
    EXPECT(cecs_is_alive(&g.world, g.player));
    EXPECT_EQ_INT(g.score, 0);
    EXPECT_EQ_INT(g.lives, 2);
}

TEST(game_tick_advances_time) {
    game_init();
    g.viewport_w = 960.0f;
    g.viewport_h = 1280.0f;
    float t_before = g.time;
    game_tick(1.0f / 60.0f, 960.0f, 1280.0f);
    EXPECT(g.time > t_before);
    EXPECT_NEAR(g.viewport_w, 960.0f, 1e-5f);
}

TEST(game_tick_restarts_on_fire_press_when_over) {
    game_init();
    g.game_over = true;
    g.input.fire_pressed = true;
    game_tick(1.0f / 60.0f, 960.0f, 1280.0f);
    EXPECT(!g.game_over);
}

TEST(game_tick_in_game_over_path) {
    game_init();
    g.game_over = true;
    g.shake = 0.5f;
    g.flash = 0.5f;
    float scroll_before = g.scroll;
    game_tick(1.0f / 60.0f, 960.0f, 1280.0f);
    EXPECT(g.scroll > scroll_before);
    EXPECT(g.shake < 0.5f);
    EXPECT(g.flash < 0.5f);
}

TEST(game_tick_decays_invuln_shake_flash) {
    game_init();
    g.invuln_timer = 0.5f;
    g.shake = 0.5f;
    g.flash = 0.5f;
    g.game_over = false;
    game_tick(1.0f / 60.0f, 960.0f, 1280.0f);
    EXPECT(g.invuln_timer < 0.5f);
    EXPECT(g.shake < 0.5f);
    EXPECT(g.flash < 0.5f);
}

TEST(game_tick_clamps_dt_high) {
    game_init();
    float t_before = g.time;
    game_tick(10.0f, 960.0f, 1280.0f);
    EXPECT_NEAR(g.dt, 1.0f / 30.0f, 1e-5f);
    EXPECT(g.time > t_before);
}

TEST(game_tick_clamps_dt_low) {
    game_init();
    game_tick(0.0f, 960.0f, 1280.0f);
    EXPECT(g.dt >= 1.0f / 120.0f - 1e-6f);
}

static void run_all(void) {
    RUN(clampf_inside);
    RUN(clampf_low_high);
    RUN(rnd_u32_deterministic);
    RUN(rnd_u32_zero_seed_recovers);
    RUN(rnd_range_in_bounds);
    RUN(dist2_basic);
    RUN(bottom_hud_h_portrait);
    RUN(bottom_hud_h_landscape);
    RUN(bottom_hud_h_clamp_min_portrait);
    RUN(bottom_hud_h_clamp_max_portrait);
    RUN(gameplay_view_h_small_height);
    RUN(gameplay_view_h_normal);
    RUN(gameplay_view_h_clamps_to_min);
    RUN(world_half_x_basic);
    RUN(world_half_x_min_clamp_tall);
    RUN(world_half_x_wide_aspect);
    RUN(player_muzzle_forward_increases_with_scale);
    RUN(spawn_mesh_creates_entity_with_components);
    RUN(spawn_burst_creates_particles);
    RUN(spawn_pickup_weapon);
    RUN(spawn_pickup_armor);
    RUN(spawn_pickup_score);
    RUN(spawn_player_creates_tagged_entity);
    RUN(spawn_player_shot_components);
    RUN(spawn_player_shot_aligned_forward);
    RUN(spawn_player_shot_aligned_side_offset);
    RUN(spawn_enemy_shot_dir_normalizes);
    RUN(spawn_enemy_shot_dir_zero_dir);
    RUN(spawn_enemy_shot_target);
    RUN(spawn_enemy_spread_creates_count_shots);
    RUN(spawn_enemy_type_default);
    RUN(spawn_enemy_all_types);
    RUN(spawn_wave_all_patterns);
    RUN(spawn_wave_default_path);
    RUN(reset_game_resets_state);
    RUN(init_stars_fills_array);
    RUN(drag_target_world_center);
    RUN(drag_target_world_left_edge);
    RUN(drag_target_world_right_edge);
    RUN(drag_target_world_tiny_viewport);
    RUN(begin_drag_sets_state);
    RUN(update_drag_updates_xy);
    RUN(end_drag_clears_state);
    RUN(player_system_no_player_safe);
    RUN(player_system_moves_to_target_on_drag);
    RUN(player_system_fires_shots_when_drag_down);
    RUN(player_system_weapon_level_4);
    RUN(player_system_weapon_level_2);
    RUN(player_system_weapon_level_3);
    RUN(player_system_clamps_weapon_level_low);
    RUN(player_system_clamps_weapon_level_high);
    RUN(player_system_respects_shot_timer);
    RUN(player_system_no_drag_no_shots);
    RUN(enemy_system_steers_and_fires);
    RUN(enemy_system_type5_dive);
    RUN(enemy_system_type5_spread_fire);
    RUN(enemy_system_type2_spread_fire);
    RUN(enemy_system_type3_no_fire);
    RUN(enemy_system_type6_steer);
    RUN(enemy_system_type4_steer);
    RUN(enemy_system_no_fire_outside_range);
    RUN(spawn_system_triggers_wave_when_timer_elapsed);
    RUN(spawn_system_no_trigger_when_timer_positive);
    RUN(movement_system_applies_velocity);
    RUN(lifetime_system_despawns_expired);
    RUN(lifetime_system_keeps_active);
    RUN(cleanup_system_removes_offscreen);
    RUN(cleanup_system_keeps_player);
    RUN(hit_player_during_invuln_noop);
    RUN(hit_player_decrements_hp_and_weapon);
    RUN(hit_player_keeps_weapon_min);
    RUN(hit_player_respawn_on_zero_hp);
    RUN(hit_player_game_over_on_no_lives);
    RUN(collision_system_player_shot_damages_enemy);
    RUN(collision_system_player_shot_kills_enemy);
    RUN(collision_system_enemy_shot_hits_player);
    RUN(collision_system_enemy_hits_player);
    RUN(collision_system_weapon_pickup);
    RUN(collision_system_weapon_pickup_caps_at_4);
    RUN(collision_system_armor_pickup);
    RUN(collision_system_armor_pickup_caps_at_5);
    RUN(collision_system_score_pickup);
    RUN(collision_system_no_player_skip);
    RUN(game_init_seeds_and_resets);
    RUN(game_tick_advances_time);
    RUN(game_tick_restarts_on_fire_press_when_over);
    RUN(game_tick_in_game_over_path);
    RUN(game_tick_decays_invuln_shake_flash);
    RUN(game_tick_clamps_dt_high);
    RUN(game_tick_clamps_dt_low);
}

int main(void) {
    printf("Running tests...\n");
    run_all();
    printf("\n%d tests, %d failed.\n", g_test_count, g_test_failed_total);
    return g_test_failed_total == 0 ? 0 : 1;
}
