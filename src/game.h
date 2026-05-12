#ifndef TRAD_GAME_H
#define TRAD_GAME_H

#include <stdbool.h>
#include <stdint.h>

/* Must agree across every TU that includes cecs.h — these macros set the
 * size of cecs_world, so a mismatch silently shifts every field that
 * follows it in `Game`. */
#define CECS_MAX_ENTITIES 960u
#define CECS_MAX_COMPONENTS 24u
#define CECS_MAX_SYSTEMS 8u
#define CECS_COMPONENT_STORAGE_BYTES 131072u
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

CECS_COMPONENT_EXTERN(Position);
CECS_COMPONENT_EXTERN(Velocity);
CECS_COMPONENT_EXTERN(Collider);
CECS_COMPONENT_EXTERN(Health);
CECS_COMPONENT_EXTERN(Lifetime);
CECS_COMPONENT_EXTERN(Damage);
CECS_COMPONENT_EXTERN(Pickup);
CECS_COMPONENT_EXTERN(Renderable);
CECS_COMPONENT_EXTERN(EnemyBrain);
CECS_COMPONENT_EXTERN(PlayerTag);
CECS_COMPONENT_EXTERN(EnemyTag);
CECS_COMPONENT_EXTERN(PlayerShotTag);
CECS_COMPONENT_EXTERN(EnemyShotTag);
CECS_COMPONENT_EXTERN(ParticleTag);
CECS_COMPONENT_EXTERN(PickupTag);

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
    float viewport_w;
    float viewport_h;
} Game;

extern Game g;

float clampf(float v, float lo, float hi);
uint32_t rnd_u32(void);
float rnd_range(float lo, float hi);
float dist2(float ax, float az, float bx, float bz);

float bottom_hud_h(void);
float gameplay_view_h(void);
float world_half_x(void);
float player_muzzle_forward_world(float shot_scale);

void add_component_types(cecs_world *world);

cecs_entity spawn_mesh(uint8_t mesh, float x, float z, float scale, float yaw);
void spawn_burst(float x, float z, int count, uint8_t r, uint8_t gg, uint8_t b);
void spawn_pickup(float x, float z, uint8_t kind);
void spawn_player(void);
void spawn_player_shot(float x, float z, float vx, float vz, float scale, int damage, uint8_t r, uint8_t gg, uint8_t b);
void spawn_player_shot_aligned(const Position *origin, float ship_heading, float side, float forward,
                               float ship_vx, float ship_vz, float spread_heading, float speed, float scale, int damage,
                               uint8_t r, uint8_t gg, uint8_t b);
void spawn_enemy_shot_dir(float x, float z, float dx, float dz, float speed, float scale);
void spawn_enemy_shot(float x, float z, float tx, float tz, float speed, float scale);
void spawn_enemy_spread(float x, float z, float tx, float tz, int count, float spread, float speed);
void spawn_enemy(float x, float z, int type);
void spawn_wave(void);

void drag_target_world(float *out_x, float *out_z);
void init_stars(void);
void reset_game(void);

void hit_player(cecs_world *world, float x, float z);

void player_system(cecs_world *world, void *ctx);
void enemy_system(cecs_world *world, void *ctx);
void spawn_system(cecs_world *world, void *ctx);
void movement_system(cecs_world *world, void *ctx);
void lifetime_system(cecs_world *world, void *ctx);
void cleanup_system(cecs_world *world, void *ctx);
void collision_system(cecs_world *world, void *ctx);

void begin_drag(bool is_touch, uintptr_t touch_id, float x, float y);
void update_drag(float x, float y);
void end_drag(void);

void game_init(void);
void game_tick(float dt, float viewport_w, float viewport_h);

#endif /* TRAD_GAME_H */
