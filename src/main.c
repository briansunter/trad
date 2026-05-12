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

#include "game.h"

typedef struct RenderState {
    sgl_pipeline solid_pip;
    sgl_pipeline alpha_pip;
} RenderState;

static RenderState rs;

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
    sgl_load_pipeline(rs.alpha_pip);
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

    sgl_load_pipeline(rs.solid_pip);
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

    sgl_load_pipeline(rs.alpha_pip);
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

    sgl_load_pipeline(rs.alpha_pip);
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

    sgl_load_pipeline(rs.solid_pip);
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
            sgl_load_pipeline(rs.alpha_pip);
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
    sgl_load_pipeline(rs.alpha_pip);
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
    rs.solid_pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .depth = { .write_enabled = true, .compare = SG_COMPAREFUNC_LESS_EQUAL },
        .cull_mode = SG_CULLMODE_NONE,
    });
    rs.alpha_pip = sgl_make_pipeline(&(sg_pipeline_desc){
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

    game_init();
}

static void frame(void) {
    game_tick((float)sapp_frame_duration(), sapp_widthf(), sapp_heightf());

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
