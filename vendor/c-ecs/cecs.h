#ifndef CECS_H
#define CECS_H

/*
 * cecs.h - a tiny, dependency-free ECS for C99 games.
 *
 * Usage:
 *
 *     #include "cecs.h"
 *
 *     In exactly one .c file:
 *
 *     #define CECS_IMPLEMENTATION
 *     #include "cecs.h"
 *
 * Override capacities before including this file when targeting small systems:
 *
 *     #define CECS_MAX_ENTITIES 32
 *     #define CECS_COMPONENT_STORAGE_BYTES 512
 *     #define CECS_IMPLEMENTATION
 *     #include "cecs.h"
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Capacity presets seed the CECS_MAX_* defaults. Define one of these before
 * including cecs.h to size the world for a target class without spelling out
 * every cap; individual CECS_MAX_* macros still win when the caller defines
 * them explicitly.
 *
 *     #define CECS_PRESET_TINY     -- ~2 KB world, embedded-class budget
 *
 * Add new presets here; do not add per-target #ifdefs anywhere else.
 */
#ifdef CECS_PRESET_TINY
#  ifndef CECS_MAX_ENTITIES
#    define CECS_MAX_ENTITIES 64u
#  endif
#  ifndef CECS_MAX_COMPONENTS
#    define CECS_MAX_COMPONENTS 8u
#  endif
#  ifndef CECS_MAX_RESOURCES
#    define CECS_MAX_RESOURCES 4u
#  endif
#  ifndef CECS_MAX_SYSTEMS
#    define CECS_MAX_SYSTEMS 8u
#  endif
#  ifndef CECS_COMPONENT_STORAGE_BYTES
#    define CECS_COMPONENT_STORAGE_BYTES 1024u
#  endif
#  ifndef CECS_RESOURCE_STORAGE_BYTES
#    define CECS_RESOURCE_STORAGE_BYTES 64u
#  endif
#endif

#ifndef CECS_MAX_ENTITIES
#define CECS_MAX_ENTITIES 128u
#endif

#ifndef CECS_MAX_COMPONENTS
#define CECS_MAX_COMPONENTS 32u
#endif

#ifndef CECS_MAX_RESOURCES
#define CECS_MAX_RESOURCES 16u
#endif

#ifndef CECS_MAX_SYSTEMS
#define CECS_MAX_SYSTEMS 32u
#endif

#ifdef CECS_MAX_QUERY_COMPONENTS
#error "CECS_MAX_QUERY_COMPONENTS was removed: queries are mask-only and may require any registered component up to CECS_MAX_COMPONENTS. See docs/adr/0007-no-query-component-cap.md"
#endif

#ifndef CECS_COMPONENT_STORAGE_BYTES
#define CECS_COMPONENT_STORAGE_BYTES 8192u
#endif

#ifndef CECS_RESOURCE_STORAGE_BYTES
#define CECS_RESOURCE_STORAGE_BYTES 512u
#endif

#if CECS_MAX_COMPONENTS > 32u
#error "cecs supports at most 32 component types because entity masks are uint32_t"
#endif

#if CECS_MAX_ENTITIES == 0u
#error "cecs requires CECS_MAX_ENTITIES to be greater than 0"
#endif

#if CECS_MAX_ENTITIES > 65534u
#error "cecs supports at most 65534 entities because entity slots are uint16_t"
#endif

#if CECS_MAX_COMPONENTS == 0u
#error "cecs requires CECS_MAX_COMPONENTS to be greater than 0"
#endif

#if CECS_MAX_RESOURCES == 0u
#error "cecs requires CECS_MAX_RESOURCES to be greater than 0"
#endif

#if CECS_MAX_RESOURCES > 255u
#error "cecs supports at most 255 resources because resource ids are uint8_t"
#endif

#if CECS_MAX_SYSTEMS == 0u
#error "cecs requires CECS_MAX_SYSTEMS to be greater than 0"
#endif

#if CECS_MAX_SYSTEMS > 255u
#error "cecs supports at most 255 systems because schedule counts are uint8_t"
#endif

#if CECS_COMPONENT_STORAGE_BYTES == 0u
#error "cecs requires CECS_COMPONENT_STORAGE_BYTES to be greater than 0"
#endif

#if CECS_RESOURCE_STORAGE_BYTES == 0u
#error "cecs requires CECS_RESOURCE_STORAGE_BYTES to be greater than 0"
#endif

#ifndef CECS_ALIGNOF
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define CECS_ALIGNOF(Type) ((uint16_t)_Alignof(Type))
#elif defined(__GNUC__) || defined(__clang__)
#define CECS_ALIGNOF(Type) ((uint16_t)__alignof__(Type))
#else
#define CECS_ALIGNOF(Type) ((uint16_t)1u)
#endif
#endif

/*
 * Branch hints for hot loops. No-op on compilers that don't speak
 * __builtin_expect; on GCC/Clang they lay out the cold arm out of line
 * so the predicted path is the fall-through. Helps embedded targets
 * without dynamic branch prediction more than it helps modern x86.
 */
#if defined(__GNUC__) || defined(__clang__)
#define CECS_LIKELY(x)   (__builtin_expect(!!(x), 1))
#define CECS_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define CECS_LIKELY(x)   (x)
#define CECS_UNLIKELY(x) (x)
#endif

#define CECS_INVALID_COMPONENT ((cecs_component_id)0xffu)
#define CECS_INVALID_RESOURCE ((cecs_resource_id)0xffu)

#define CECS_ARRAY_COUNT(Array) (sizeof(Array) / sizeof((Array)[0]))

#define CECS_COMPONENT_ID(Type) Type##_component_id
#define CECS_COMPONENT_DECLARE(Type) cecs_component_id CECS_COMPONENT_ID(Type) = CECS_INVALID_COMPONENT
#define CECS_COMPONENT_EXTERN(Type) extern cecs_component_id CECS_COMPONENT_ID(Type)
#define CECS_COMPONENT(World, Type) \
    (CECS_COMPONENT_ID(Type) = cecs_component_register((World), (uint16_t)sizeof(Type), CECS_ALIGNOF(Type)))
#define CECS_TAG_DECLARE(Type) CECS_COMPONENT_DECLARE(Type)
#define CECS_TAG_EXTERN(Type) CECS_COMPONENT_EXTERN(Type)
#define CECS_TAG(World, Type) \
    (CECS_COMPONENT_ID(Type) = cecs_component_register((World), 0u, 1u))
#define CECS_ADD(World, Entity, Type, ValuePtr) \
    cecs_add((World), (Entity), CECS_COMPONENT_ID(Type), (ValuePtr))
#define CECS_GET(World, Entity, Type) \
    ((Type *)cecs_get((World), (Entity), CECS_COMPONENT_ID(Type)))
#define CECS_HAS(World, Entity, Type) \
    cecs_has((World), (Entity), CECS_COMPONENT_ID(Type))
#define CECS_REMOVE(World, Entity, Type) \
    cecs_remove((World), (Entity), CECS_COMPONENT_ID(Type))
#define CECS_QUERY_GET(Query, Type) \
    ((Type *)cecs_query_get((Query), CECS_COMPONENT_ID(Type)))
#define CECS_QUERY_WITHOUT(Query, Type) \
    cecs_query_without((Query), CECS_COMPONENT_ID(Type))
#define CECS_FILTER_REQUIRE(World, Filter, Type) \
    cecs_filter_require((World), (Filter), CECS_COMPONENT_ID(Type))
#define CECS_FILTER_WITHOUT(World, Filter, Type) \
    cecs_filter_without((World), (Filter), CECS_COMPONENT_ID(Type))

/*
 * Fastpath component access. Expands to direct arena indexing — no
 * function call, no liveness check, no component-validity check. Use
 * inside a query loop (where the cursor has already filtered for live
 * entities holding the required components) or when the caller has
 * otherwise proven the precondition. Reach for CECS_GET / CECS_QUERY_GET
 * when those guarantees aren't already in hand.
 */
#define CECS_GET_FAST(World, Entity, Type) \
    ((Type *)((uint8_t *)(World)->component_storage.bytes \
              + (World)->components[CECS_COMPONENT_ID(Type)].offset \
              + (uint32_t)(Entity).slot \
                * (uint32_t)(World)->components[CECS_COMPONENT_ID(Type)].stride))
#define CECS_QUERY_GET_FAST(Query, Type) \
    CECS_GET_FAST((Query)->world, (Query)->entity, Type)

/*
 * Fastpath presence check. Direct mask byte read + bit AND, no function
 * call, no validity check. Same precondition as CECS_GET_FAST: caller
 * has proven the entity is alive (typical pattern: optional component
 * lookup inside a CECS_QUERY_EACH body where the cursor already vouches
 * for liveness).
 */
#define CECS_HAS_FAST(World, Entity, Type) \
    (((World)->masks[(Entity).slot] \
      & ((cecs_mask)1u << CECS_COMPONENT_ID(Type))) != (cecs_mask)0u)
#define CECS_QUERY_HAS_FAST(Query, Type) \
    CECS_HAS_FAST((Query)->world, (Query)->entity, Type)

/*
 * Inline iteration over matching entities. Replaces a function-pointer
 * callback form: the body is expanded straight into the loop, so the
 * compiler can hoist constant lookups and inline CECS_QUERY_GET_FAST.
 *
 *     CECS_QUERY_EACH(&world,
 *         ((cecs_component_id[]){ CECS_COMPONENT_ID(Position),
 *                                 CECS_COMPONENT_ID(Velocity) }), {
 *             Position *p = CECS_QUERY_GET_FAST(&_q, Position);
 *             Velocity *v = CECS_QUERY_GET_FAST(&_q, Velocity);
 *             p->x = (int16_t)(p->x + v->x);
 *         });
 *
 * The cursor is exposed inside the body as `_q` (read its `entity` field
 * for the current entity). `break` ends the iteration; `continue` skips
 * to the next entity. The component-id array must be a compound literal
 * or a sized array — pointers lose the size that drives sizeof().
 *
 * See ADR 0005 — Hot-path query is a macro, not a function.
 */
#define CECS_QUERY_EACH(WorldPtr, ComponentIds, ...) \
    do { \
        cecs_query _q; \
        const cecs_component_id *_q_ids = (ComponentIds); \
        uint8_t _q_count = (uint8_t)(sizeof(ComponentIds) / sizeof((ComponentIds)[0])); \
        uint8_t _q_i; \
        uint8_t _q_ok = 1u; \
        cecs_query_init(&_q, (WorldPtr)); \
        for (_q_i = 0u; _q_i < _q_count; _q_i++) { \
            if (cecs_query_require(&_q, _q_ids[_q_i]) != CECS_OK) { \
                _q_ok = 0u; \
                break; \
            } \
        } \
        while (_q_ok && cecs_query_next(&_q)) { \
            __VA_ARGS__ \
        } \
    } while (0)

#define CECS_QUERY_EACH_WITHOUT(WorldPtr, ComponentIds, ExcludeIds, ...) \
    do { \
        cecs_query _q; \
        const cecs_component_id *_q_ids = (ComponentIds); \
        const cecs_component_id *_q_without_ids = (ExcludeIds); \
        uint8_t _q_count = (uint8_t)(sizeof(ComponentIds) / sizeof((ComponentIds)[0])); \
        uint8_t _q_without_count = (uint8_t)(sizeof(ExcludeIds) / sizeof((ExcludeIds)[0])); \
        uint8_t _q_i; \
        uint8_t _q_ok = 1u; \
        cecs_query_init(&_q, (WorldPtr)); \
        for (_q_i = 0u; _q_i < _q_count; _q_i++) { \
            if (cecs_query_require(&_q, _q_ids[_q_i]) != CECS_OK) { \
                _q_ok = 0u; \
                break; \
            } \
        } \
        for (_q_i = 0u; _q_ok && _q_i < _q_without_count; _q_i++) { \
            if (cecs_query_without(&_q, _q_without_ids[_q_i]) != CECS_OK) { \
                _q_ok = 0u; \
                break; \
            } \
        } \
        while (_q_ok && cecs_query_next(&_q)) { \
            __VA_ARGS__ \
        } \
    } while (0)

#define CECS_QUERY_EACH_FILTER(WorldPtr, FilterPtr, ...) \
    do { \
        cecs_query _q; \
        cecs_query_init_filter(&_q, (WorldPtr), (FilterPtr)); \
        while (cecs_query_next(&_q)) { \
            __VA_ARGS__ \
        } \
    } while (0)

#define CECS_RESOURCE_ID(Type) Type##_resource_id
#define CECS_RESOURCE_DECLARE(Type) cecs_resource_id CECS_RESOURCE_ID(Type) = CECS_INVALID_RESOURCE
#define CECS_RESOURCE_EXTERN(Type) extern cecs_resource_id CECS_RESOURCE_ID(Type)
#define CECS_RESOURCE(World, Type) \
    (CECS_RESOURCE_ID(Type) = cecs_resource_register((World), (uint16_t)sizeof(Type), CECS_ALIGNOF(Type)))
#define CECS_RESOURCE_SET(World, Type, ValuePtr) \
    cecs_resource_set((World), CECS_RESOURCE_ID(Type), (ValuePtr))
#define CECS_RESOURCE_GET(World, Type) \
    ((Type *)cecs_resource_get((World), CECS_RESOURCE_ID(Type)))

/*
 * Fastpath resource access. Expands to a single offset+cast — no function
 * call, no validity check. Safe whenever the caller knows the resource has
 * been registered (typical hot-path pattern: resource registered at world
 * setup, read every frame thereafter).
 */
#define CECS_RESOURCE_GET_FAST(World, Type) \
    ((Type *)((uint8_t *)(World)->resource_storage.bytes \
              + (World)->resources[CECS_RESOURCE_ID(Type)].offset))

#define CECS_COMPONENT_TYPE(Type) \
    { CECS_TYPE_COMPONENT, (uint16_t)sizeof(Type), CECS_ALIGNOF(Type), &CECS_COMPONENT_ID(Type) }
#define CECS_TAG_TYPE(Type) \
    { CECS_TYPE_COMPONENT, 0u, 1u, &CECS_COMPONENT_ID(Type) }
#define CECS_RESOURCE_TYPE(Type) \
    { CECS_TYPE_RESOURCE, (uint16_t)sizeof(Type), CECS_ALIGNOF(Type), &CECS_RESOURCE_ID(Type) }
#define CECS_REGISTER_TYPES(World, Types) \
    cecs_register_types((World), (Types), (uint8_t)CECS_ARRAY_COUNT(Types))

enum {
    CECS_OK = 0,
    CECS_ERR_FULL = -1,
    CECS_ERR_INVALID = -2,
    CECS_ERR_DEAD = -3,
    CECS_ERR_STORAGE = -4
};

typedef uint8_t cecs_component_id;
typedef uint8_t cecs_resource_id;

/*
 * cecs_mask is the per-entity component-presence bitmap. Its width follows
 * CECS_MAX_COMPONENTS so an embedded build with 8 components pays one byte
 * per entity (not four) and emits single-byte AND/OR/test ops on 8-bit
 * targets. uint32_t is the ceiling, not the floor — see ADR 0002.
 */
#if CECS_MAX_COMPONENTS <= 8u
typedef uint8_t cecs_mask;
#elif CECS_MAX_COMPONENTS <= 16u
typedef uint16_t cecs_mask;
#else
typedef uint32_t cecs_mask;
#endif

/*
 * cecs_slot_index and cecs_generation widths follow CECS_MAX_ENTITIES.
 * Entity handles, free-list links, and the generation array all narrow
 * together. The skip-0-on-wrap rule still works at uint8_t (255 useful
 * generations before each wrap).
 */
#if CECS_MAX_ENTITIES <= 255u
typedef uint8_t cecs_slot_index;
typedef uint8_t cecs_generation;
#define CECS_INVALID_SLOT ((cecs_slot_index)0xffu)
#else
typedef uint16_t cecs_slot_index;
typedef uint16_t cecs_generation;
#define CECS_INVALID_SLOT ((cecs_slot_index)0xffffu)
#endif

typedef enum cecs_type_kind {
    CECS_TYPE_COMPONENT = 1,
    CECS_TYPE_RESOURCE = 2
} cecs_type_kind;

typedef struct cecs_entity {
    cecs_slot_index slot;
    cecs_generation generation;
} cecs_entity;

typedef struct cecs_type_registration {
    cecs_type_kind kind;
    uint16_t size;
    uint16_t align;
    uint8_t *out_id;
} cecs_type_registration;

typedef struct cecs_arena {
    uint32_t capacity;
    uint32_t used;
} cecs_arena;

typedef struct cecs_storage_desc {
    uint16_t size;
    uint16_t align;
    uint16_t stride;
    uint32_t offset;
} cecs_storage_desc;

typedef cecs_storage_desc cecs_component_desc;
typedef cecs_storage_desc cecs_resource_desc;

typedef union cecs_component_storage {
    uint8_t bytes[CECS_COMPONENT_STORAGE_BYTES];
    void *as_ptr;
    long as_long;
    double as_double;
} cecs_component_storage;

typedef union cecs_resource_storage {
    uint8_t bytes[CECS_RESOURCE_STORAGE_BYTES];
    void *as_ptr;
    long as_long;
    double as_double;
} cecs_resource_storage;

/*
 * Internal: the slot allocator owns entity lifetime — free-list slots,
 * generations (with skip-0-on-wrap), and the live count. Despawn cleanup
 * of component storage stays in cecs_despawn; this struct knows nothing
 * about components or masks.
 */
typedef struct cecs_slots {
    uint8_t alive[CECS_MAX_ENTITIES];
    cecs_generation generations[CECS_MAX_ENTITIES];
    cecs_slot_index free_next[CECS_MAX_ENTITIES];
    cecs_slot_index free_head;
    cecs_slot_index live_count;
    /*
     * One past the highest slot ever returned by cecs_slots_acquire.
     * Monotonic: never shrinks on despawn. Queries iterate [0, high_water)
     * so a small working set in a large CECS_MAX_ENTITIES world avoids
     * scanning empty tail slots.
     */
    cecs_slot_index high_water;
} cecs_slots;

typedef struct cecs_world {
    cecs_slots slots;
    cecs_mask masks[CECS_MAX_ENTITIES];

    uint8_t component_count;
    cecs_arena component_arena;
    cecs_component_desc components[CECS_MAX_COMPONENTS];
    cecs_component_storage component_storage;

    uint8_t resource_count;
    cecs_arena resource_arena;
    cecs_resource_desc resources[CECS_MAX_RESOURCES];
    cecs_resource_storage resource_storage;
} cecs_world;

typedef struct cecs_filter {
    cecs_mask include_mask;
    cecs_mask exclude_mask;
} cecs_filter;

/*
 * Filters and queries are just include/exclude masks. Queries scan entity
 * slots in ascending order. World mutations are allowed during query callbacks:
 * changes to slots already visited are not revisited, and changes to slots not
 * yet visited are observed when those slots are reached. Nested queries keep
 * independent cursors.
 */
typedef struct cecs_query {
    cecs_world *world;
    cecs_mask include_mask;
    cecs_mask exclude_mask;
    cecs_slot_index next_slot;
    cecs_entity entity;
} cecs_query;

typedef void (*cecs_system_fn)(cecs_world *world, void *ctx);

typedef struct cecs_system {
    cecs_system_fn fn;
    void *ctx;
    uint8_t enabled;
} cecs_system;

typedef struct cecs_schedule {
    uint8_t count;
    cecs_system systems[CECS_MAX_SYSTEMS];
} cecs_schedule;

#ifdef __cplusplus
extern "C" {
#endif

cecs_entity cecs_entity_null(void);
uint8_t cecs_entity_is_null(cecs_entity entity);

void cecs_world_init(cecs_world *world);
uint16_t cecs_live_count(const cecs_world *world);

cecs_entity cecs_spawn(cecs_world *world);
int cecs_despawn(cecs_world *world, cecs_entity entity);
int cecs_clear(cecs_world *world, cecs_entity entity);
cecs_entity cecs_clone(cecs_world *world, cecs_entity source);
int cecs_clone_into(cecs_world *world, cecs_entity target, cecs_entity source);
uint8_t cecs_is_alive(const cecs_world *world, cecs_entity entity);

cecs_component_id cecs_component_register(cecs_world *world, uint16_t size, uint16_t align);
int cecs_add(cecs_world *world, cecs_entity entity, cecs_component_id component, const void *value);
void *cecs_get(cecs_world *world, cecs_entity entity, cecs_component_id component);
const void *cecs_get_const(const cecs_world *world, cecs_entity entity, cecs_component_id component);
uint8_t cecs_has(const cecs_world *world, cecs_entity entity, cecs_component_id component);
int cecs_remove(cecs_world *world, cecs_entity entity, cecs_component_id component);

cecs_resource_id cecs_resource_register(cecs_world *world, uint16_t size, uint16_t align);
int cecs_resource_set(cecs_world *world, cecs_resource_id resource, const void *value);
void *cecs_resource_get(cecs_world *world, cecs_resource_id resource);
const void *cecs_resource_get_const(const cecs_world *world, cecs_resource_id resource);

int cecs_register_type(cecs_world *world, const cecs_type_registration *type);
int cecs_register_types(cecs_world *world, const cecs_type_registration *types, uint8_t count);

void cecs_filter_init(cecs_filter *filter);
int cecs_filter_require(const cecs_world *world, cecs_filter *filter, cecs_component_id component);
int cecs_filter_without(const cecs_world *world, cecs_filter *filter, cecs_component_id component);

void cecs_query_init(cecs_query *query, cecs_world *world);
void cecs_query_init_filter(cecs_query *query, cecs_world *world, const cecs_filter *filter);
int cecs_query_require(cecs_query *query, cecs_component_id component);
int cecs_query_without(cecs_query *query, cecs_component_id component);
uint8_t cecs_query_next(cecs_query *query);
void *cecs_query_get(cecs_query *query, cecs_component_id component);

void cecs_schedule_init(cecs_schedule *schedule);
int cecs_schedule_add(cecs_schedule *schedule, cecs_system_fn fn, void *ctx);
uint8_t cecs_schedule_count(const cecs_schedule *schedule);
int cecs_schedule_enable(cecs_schedule *schedule, uint8_t index, uint8_t enabled);
uint8_t cecs_schedule_is_enabled(const cecs_schedule *schedule, uint8_t index);
uint8_t cecs_schedule_run(cecs_schedule *schedule, cecs_world *world);

#ifdef __cplusplus
}
#endif

#endif /* CECS_H */

#ifdef CECS_IMPLEMENTATION
#ifndef CECS_IMPLEMENTATION_ONCE
#define CECS_IMPLEMENTATION_ONCE

#include <string.h>

static int cecs_align_forward_u32(uint32_t value, uint16_t align, uint32_t *out_value) {
    uint32_t add;
    uint32_t remainder;
    if (out_value == NULL) return CECS_ERR_INVALID;
    if (align <= 1u) {
        *out_value = value;
        return CECS_OK;
    }
    remainder = value % (uint32_t)align;
    if (remainder == 0u) {
        *out_value = value;
        return CECS_OK;
    }
    add = (uint32_t)align - remainder;
    if (value > (UINT32_MAX - add)) return CECS_ERR_STORAGE;
    *out_value = value + add;
    return CECS_OK;
}

static int cecs_stride_for(uint16_t size, uint16_t align, uint16_t *out_stride) {
    uint32_t stride;
    if (out_stride == NULL) return CECS_ERR_INVALID;
    if (cecs_align_forward_u32((uint32_t)size, align, &stride) != CECS_OK) return CECS_ERR_STORAGE;
    if (stride > (uint32_t)UINT16_MAX) return CECS_ERR_STORAGE;
    *out_stride = (uint16_t)stride;
    return CECS_OK;
}

static void cecs_arena_init(cecs_arena *arena, uint32_t capacity) {
    if (arena == NULL) return;
    arena->capacity = capacity;
    arena->used = 0u;
}

static int cecs_arena_reserve(cecs_arena *arena, uint32_t size, uint16_t align, uint32_t *out_offset) {
    uint32_t offset;
    if (arena == NULL || out_offset == NULL) return CECS_ERR_INVALID;
    if (size == 0u) return CECS_ERR_INVALID;
    if (align == 0u) align = 1u;
    if (cecs_align_forward_u32(arena->used, align, &offset) != CECS_OK) return CECS_ERR_STORAGE;
    if (offset > arena->capacity) return CECS_ERR_STORAGE;
    if (size > (arena->capacity - offset)) return CECS_ERR_STORAGE;
    arena->used = offset + size;
    *out_offset = offset;
    return CECS_OK;
}

static void *cecs_arena_at(void *bytes, uint32_t offset) {
    return (void *)((uint8_t *)bytes + offset);
}

static const void *cecs_arena_at_const(const void *bytes, uint32_t offset) {
    return (const void *)((const uint8_t *)bytes + offset);
}

static uint8_t cecs_component_valid(const cecs_world *world, cecs_component_id component) {
    if (world == NULL) return 0u;
    return (uint8_t)(component < world->component_count);
}

static uint8_t cecs_resource_valid(const cecs_world *world, cecs_resource_id resource) {
    if (world == NULL) return 0u;
    return (uint8_t)(resource < world->resource_count);
}

static cecs_mask cecs_component_bit(cecs_component_id component) {
    return (cecs_mask)((cecs_mask)1u << component);
}

static void *cecs_component_ptr(cecs_world *world, cecs_component_id component, cecs_slot_index slot) {
    const cecs_component_desc *desc;
    desc = &world->components[component];
    return cecs_arena_at(
        world->component_storage.bytes,
        desc->offset + ((uint32_t)slot * (uint32_t)desc->stride));
}

static const void *cecs_component_ptr_const(const cecs_world *world, cecs_component_id component, cecs_slot_index slot) {
    const cecs_component_desc *desc;
    desc = &world->components[component];
    return cecs_arena_at_const(
        world->component_storage.bytes,
        desc->offset + ((uint32_t)slot * (uint32_t)desc->stride));
}

static void *cecs_resource_ptr(cecs_world *world, cecs_resource_id resource) {
    return cecs_arena_at(world->resource_storage.bytes, world->resources[resource].offset);
}

static const void *cecs_resource_ptr_const(const cecs_world *world, cecs_resource_id resource) {
    return cecs_arena_at_const(world->resource_storage.bytes, world->resources[resource].offset);
}

static void cecs_slots_init(cecs_slots *slots) {
    cecs_slot_index i;
    slots->free_head = 0u;
    slots->live_count = 0u;
    slots->high_water = 0u;
    for (i = 0u; i < CECS_MAX_ENTITIES; i++) {
        slots->alive[i] = 0u;
        slots->generations[i] = 1u;
        slots->free_next[i] = (cecs_slot_index)(i + 1u);
    }
    slots->free_next[CECS_MAX_ENTITIES - 1u] = CECS_INVALID_SLOT;
}

static cecs_entity cecs_slots_acquire(cecs_slots *slots) {
    cecs_slot_index slot;
    cecs_slot_index reach;
    cecs_entity entity;
    entity.slot = CECS_INVALID_SLOT;
    entity.generation = 0u;
    if (slots->free_head == CECS_INVALID_SLOT) return entity;
    slot = slots->free_head;
    slots->free_head = slots->free_next[slot];
    slots->free_next[slot] = CECS_INVALID_SLOT;
    slots->alive[slot] = 1u;
    slots->live_count++;
    reach = (cecs_slot_index)(slot + 1u);
    if (reach > slots->high_water) slots->high_water = reach;
    entity.slot = slot;
    entity.generation = slots->generations[slot];
    return entity;
}

static void cecs_slots_release(cecs_slots *slots, cecs_slot_index slot) {
    slots->alive[slot] = 0u;
    if (slots->live_count) slots->live_count--;
    slots->generations[slot]++;
    /* Generation 0 is the null-entity sentinel; skip it on wrap. */
    if (slots->generations[slot] == 0u) slots->generations[slot] = 1u;
    slots->free_next[slot] = slots->free_head;
    slots->free_head = slot;
}

static uint8_t cecs_slots_is_live(const cecs_slots *slots, cecs_entity entity) {
    if (entity.slot >= CECS_MAX_ENTITIES) return 0u;
    if (!slots->alive[entity.slot]) return 0u;
    return (uint8_t)(slots->generations[entity.slot] == entity.generation);
}

cecs_entity cecs_entity_null(void) {
    cecs_entity entity;
    entity.slot = CECS_INVALID_SLOT;
    entity.generation = 0u;
    return entity;
}

uint8_t cecs_entity_is_null(cecs_entity entity) {
    return (uint8_t)(entity.slot == CECS_INVALID_SLOT);
}

void cecs_world_init(cecs_world *world) {
    if (world == NULL) return;
    memset(world, 0, sizeof(*world));
    cecs_slots_init(&world->slots);
    cecs_arena_init(&world->component_arena, (uint32_t)CECS_COMPONENT_STORAGE_BYTES);
    cecs_arena_init(&world->resource_arena, (uint32_t)CECS_RESOURCE_STORAGE_BYTES);
}

uint16_t cecs_live_count(const cecs_world *world) {
    if (world == NULL) return 0u;
    return world->slots.live_count;
}

cecs_entity cecs_spawn(cecs_world *world) {
    cecs_entity entity;
    if (world == NULL) return cecs_entity_null();
    entity = cecs_slots_acquire(&world->slots);
    if (!cecs_entity_is_null(entity)) world->masks[entity.slot] = 0u;
    return entity;
}

uint8_t cecs_is_alive(const cecs_world *world, cecs_entity entity) {
    if (world == NULL) return 0u;
    return cecs_slots_is_live(&world->slots, entity);
}

int cecs_despawn(cecs_world *world, cecs_entity entity) {
    if (!cecs_is_alive(world, entity)) return CECS_ERR_DEAD;
    /*
     * The mask is the source of truth for component presence; clearing
     * it makes cecs_get / cecs_has / cecs_query_next see the entity as
     * empty. Storage bytes are reused on the next add — overwriting them
     * here would burn cycles for no observable effect under the public
     * contract. CECS_GET_FAST on a despawned entity is already a
     * precondition violation; whether it sees zeros or stale bytes does
     * not change that.
     */
    world->masks[entity.slot] = (cecs_mask)0u;
    cecs_slots_release(&world->slots, entity.slot);
    return CECS_OK;
}

int cecs_clear(cecs_world *world, cecs_entity entity) {
    if (!cecs_is_alive(world, entity)) return CECS_ERR_DEAD;
    world->masks[entity.slot] = (cecs_mask)0u;
    return CECS_OK;
}

static void cecs_copy_present_components(
    cecs_world *world,
    cecs_slot_index target_slot,
    cecs_slot_index source_slot,
    cecs_mask mask) {
    uint8_t component;
    for (component = 0u; component < world->component_count; component++) {
        const cecs_component_desc *desc;
        cecs_mask bit;
        bit = cecs_component_bit(component);
        if ((mask & bit) == 0u) continue;
        desc = &world->components[component];
        if (desc->size == 0u) continue;
        memcpy(
            cecs_component_ptr(world, component, target_slot),
            cecs_component_ptr(world, component, source_slot),
            desc->size);
    }
}

cecs_entity cecs_clone(cecs_world *world, cecs_entity source) {
    cecs_entity target;
    cecs_mask source_mask;
    if (!cecs_is_alive(world, source)) return cecs_entity_null();
    target = cecs_spawn(world);
    if (cecs_entity_is_null(target)) return target;
    source_mask = world->masks[source.slot];
    cecs_copy_present_components(world, target.slot, source.slot, source_mask);
    world->masks[target.slot] = source_mask;
    return target;
}

int cecs_clone_into(cecs_world *world, cecs_entity target, cecs_entity source) {
    cecs_mask source_mask;
    if (!cecs_is_alive(world, target)) return CECS_ERR_DEAD;
    if (!cecs_is_alive(world, source)) return CECS_ERR_DEAD;
    if (target.slot == source.slot && target.generation == source.generation) return CECS_OK;
    source_mask = world->masks[source.slot];
    cecs_copy_present_components(world, target.slot, source.slot, source_mask);
    world->masks[target.slot] = source_mask;
    return CECS_OK;
}

/*
 * Reserve `slab_count` strided cells for a typed storage area in `arena`,
 * record the descriptor at descs[*count], and return the assigned id via
 * out_id. Components pass slab_count=CECS_MAX_ENTITIES; resources pass 1.
 * Zero-sized components are tags: they install a descriptor and mask bit
 * without reserving arena bytes.
 * Returns CECS_OK / CECS_ERR_INVALID / CECS_ERR_FULL / CECS_ERR_STORAGE.
 */
static int cecs_storage_install(
    cecs_arena *arena,
    cecs_storage_desc *descs,
    uint8_t *count,
    uint8_t max_count,
    uint16_t size,
    uint16_t align,
    uint16_t slab_count,
    uint8_t allow_zero_size,
    uint8_t *out_id) {
    uint16_t stride;
    uint32_t bytes_needed;
    uint32_t offset;
    uint8_t id;
    if (arena == NULL || descs == NULL || count == NULL || out_id == NULL) return CECS_ERR_INVALID;
    if (slab_count == 0u) return CECS_ERR_INVALID;
    if (*count >= max_count) return CECS_ERR_FULL;
    if (align == 0u) align = 1u;
    if (size == 0u) {
        if (!allow_zero_size) return CECS_ERR_INVALID;
        id = *count;
        descs[id].size = 0u;
        descs[id].align = 1u;
        descs[id].stride = 0u;
        descs[id].offset = arena->used;
        *count = (uint8_t)(id + 1u);
        *out_id = id;
        return CECS_OK;
    }
    if (slab_count > 1u) {
        if (cecs_stride_for(size, align, &stride) != CECS_OK) return CECS_ERR_STORAGE;
    } else {
        stride = size;
    }
    bytes_needed = (uint32_t)stride * (uint32_t)slab_count;
    if (cecs_arena_reserve(arena, bytes_needed, align, &offset) != CECS_OK) return CECS_ERR_STORAGE;
    id = *count;
    descs[id].size = size;
    descs[id].align = align;
    descs[id].stride = stride;
    descs[id].offset = offset;
    *count = (uint8_t)(id + 1u);
    *out_id = id;
    return CECS_OK;
}

cecs_component_id cecs_component_register(cecs_world *world, uint16_t size, uint16_t align) {
    uint8_t id;
    if (world == NULL) return CECS_INVALID_COMPONENT;
    if (cecs_storage_install(
            &world->component_arena,
            world->components,
            &world->component_count,
            (uint8_t)CECS_MAX_COMPONENTS,
            size, align,
            (uint16_t)CECS_MAX_ENTITIES,
            1u,
            &id) != CECS_OK) {
        return CECS_INVALID_COMPONENT;
    }
    return (cecs_component_id)id;
}

int cecs_add(cecs_world *world, cecs_entity entity, cecs_component_id component, const void *value) {
    if (!cecs_is_alive(world, entity)) return CECS_ERR_DEAD;
    if (!cecs_component_valid(world, component)) return CECS_ERR_INVALID;
    if (world->components[component].size != 0u) {
        void *target;
        target = cecs_component_ptr(world, component, entity.slot);
        if (value != NULL) {
            memcpy(target, value, world->components[component].size);
        } else {
            memset(target, 0, world->components[component].size);
        }
    }
    world->masks[entity.slot] |= cecs_component_bit(component);
    return CECS_OK;
}

void *cecs_get(cecs_world *world, cecs_entity entity, cecs_component_id component) {
    if (!cecs_has(world, entity, component)) return NULL;
    if (world->components[component].size == 0u) return NULL;
    return cecs_component_ptr(world, component, entity.slot);
}

const void *cecs_get_const(const cecs_world *world, cecs_entity entity, cecs_component_id component) {
    if (!cecs_has(world, entity, component)) return NULL;
    if (world->components[component].size == 0u) return NULL;
    return cecs_component_ptr_const(world, component, entity.slot);
}

uint8_t cecs_has(const cecs_world *world, cecs_entity entity, cecs_component_id component) {
    if (!cecs_is_alive(world, entity)) return 0u;
    if (!cecs_component_valid(world, component)) return 0u;
    return (uint8_t)((world->masks[entity.slot] & cecs_component_bit(component)) != 0u);
}

int cecs_remove(cecs_world *world, cecs_entity entity, cecs_component_id component) {
    if (!cecs_is_alive(world, entity)) return CECS_ERR_DEAD;
    if (!cecs_component_valid(world, component)) return CECS_ERR_INVALID;
    if ((world->masks[entity.slot] & cecs_component_bit(component)) == 0u) return CECS_OK;
    /* Mask clear is the only state that matters; storage bytes stay until re-added (see cecs_despawn). */
    world->masks[entity.slot] &= ~cecs_component_bit(component);
    return CECS_OK;
}

cecs_resource_id cecs_resource_register(cecs_world *world, uint16_t size, uint16_t align) {
    uint8_t id;
    if (world == NULL) return CECS_INVALID_RESOURCE;
    if (cecs_storage_install(
            &world->resource_arena,
            world->resources,
            &world->resource_count,
            (uint8_t)CECS_MAX_RESOURCES,
            size, align,
            1u,
            0u,
            &id) != CECS_OK) {
        return CECS_INVALID_RESOURCE;
    }
    memset(cecs_resource_ptr(world, (cecs_resource_id)id), 0, size);
    return (cecs_resource_id)id;
}

int cecs_register_type(cecs_world *world, const cecs_type_registration *type) {
    uint8_t id;
    int status;
    if (world == NULL || type == NULL || type->out_id == NULL) return CECS_ERR_INVALID;
    if (type->kind == CECS_TYPE_COMPONENT) {
        status = cecs_storage_install(
            &world->component_arena,
            world->components,
            &world->component_count,
            (uint8_t)CECS_MAX_COMPONENTS,
            type->size, type->align,
            (uint16_t)CECS_MAX_ENTITIES,
            1u,
            &id);
        if (status != CECS_OK) return status;
        *type->out_id = id;
        return CECS_OK;
    }
    if (type->kind == CECS_TYPE_RESOURCE) {
        if (type->size == 0u) return CECS_ERR_INVALID;
        status = cecs_storage_install(
            &world->resource_arena,
            world->resources,
            &world->resource_count,
            (uint8_t)CECS_MAX_RESOURCES,
            type->size, type->align,
            1u,
            0u,
            &id);
        if (status != CECS_OK) return status;
        memset(cecs_resource_ptr(world, (cecs_resource_id)id), 0, type->size);
        *type->out_id = id;
        return CECS_OK;
    }
    return CECS_ERR_INVALID;
}

int cecs_register_types(cecs_world *world, const cecs_type_registration *types, uint8_t count) {
    uint8_t i;
    if (world == NULL) return CECS_ERR_INVALID;
    if (count == 0u) return CECS_OK;
    if (types == NULL) return CECS_ERR_INVALID;
    for (i = 0u; i < count; i++) {
        int status;
        status = cecs_register_type(world, &types[i]);
        if (status != CECS_OK) return status;
    }
    return CECS_OK;
}

int cecs_resource_set(cecs_world *world, cecs_resource_id resource, const void *value) {
    void *target;
    if (!cecs_resource_valid(world, resource)) return CECS_ERR_INVALID;
    target = cecs_resource_ptr(world, resource);
    if (value != NULL) {
        memcpy(target, value, world->resources[resource].size);
    } else {
        memset(target, 0, world->resources[resource].size);
    }
    return CECS_OK;
}

void *cecs_resource_get(cecs_world *world, cecs_resource_id resource) {
    if (world == NULL) return NULL;
    if (resource >= world->resource_count) return NULL;
    return cecs_resource_ptr(world, resource);
}

const void *cecs_resource_get_const(const cecs_world *world, cecs_resource_id resource) {
    if (world == NULL) return NULL;
    if (resource >= world->resource_count) return NULL;
    return cecs_resource_ptr_const(world, resource);
}

void cecs_filter_init(cecs_filter *filter) {
    if (filter == NULL) return;
    filter->include_mask = (cecs_mask)0u;
    filter->exclude_mask = (cecs_mask)0u;
}

int cecs_filter_require(const cecs_world *world, cecs_filter *filter, cecs_component_id component) {
    cecs_mask bit;
    if (filter == NULL) return CECS_ERR_INVALID;
    if (!cecs_component_valid(world, component)) return CECS_ERR_INVALID;
    bit = cecs_component_bit(component);
    if ((filter->exclude_mask & bit) != 0u) return CECS_ERR_INVALID;
    filter->include_mask |= bit;
    return CECS_OK;
}

int cecs_filter_without(const cecs_world *world, cecs_filter *filter, cecs_component_id component) {
    cecs_mask bit;
    if (filter == NULL) return CECS_ERR_INVALID;
    if (!cecs_component_valid(world, component)) return CECS_ERR_INVALID;
    bit = cecs_component_bit(component);
    if ((filter->include_mask & bit) != 0u) return CECS_ERR_INVALID;
    filter->exclude_mask |= bit;
    return CECS_OK;
}

void cecs_query_init(cecs_query *query, cecs_world *world) {
    if (query == NULL) return;
    query->world = world;
    query->include_mask = (cecs_mask)0u;
    query->exclude_mask = (cecs_mask)0u;
    query->next_slot = 0u;
    query->entity = cecs_entity_null();
}

void cecs_query_init_filter(cecs_query *query, cecs_world *world, const cecs_filter *filter) {
    if (query == NULL) return;
    query->world = world;
    if (filter != NULL) {
        query->include_mask = filter->include_mask;
        query->exclude_mask = filter->exclude_mask;
    } else {
        query->include_mask = (cecs_mask)0u;
        query->exclude_mask = (cecs_mask)0u;
    }
    query->next_slot = 0u;
    query->entity = cecs_entity_null();
}

int cecs_query_require(cecs_query *query, cecs_component_id component) {
    cecs_mask bit;
    if (query == NULL) return CECS_ERR_INVALID;
    if (!cecs_component_valid(query->world, component)) return CECS_ERR_INVALID;
    bit = cecs_component_bit(component);
    if ((query->exclude_mask & bit) != 0u) return CECS_ERR_INVALID;
    query->include_mask |= bit;
    return CECS_OK;
}

int cecs_query_without(cecs_query *query, cecs_component_id component) {
    cecs_mask bit;
    if (query == NULL) return CECS_ERR_INVALID;
    if (!cecs_component_valid(query->world, component)) return CECS_ERR_INVALID;
    bit = cecs_component_bit(component);
    if ((query->include_mask & bit) != 0u) return CECS_ERR_INVALID;
    query->exclude_mask |= bit;
    return CECS_OK;
}

uint8_t cecs_query_next(cecs_query *query) {
    cecs_world *world;
    if (query == NULL) return 0u;
    world = query->world;
    if (world == NULL) return 0u;
    while (query->next_slot < world->slots.high_water) {
        cecs_slot_index slot;
        cecs_mask mask;
        slot = query->next_slot;
        query->next_slot++;
        if (CECS_UNLIKELY(!world->slots.alive[slot])) continue;
        mask = world->masks[slot];
        if (CECS_UNLIKELY((mask & query->include_mask) != query->include_mask)) continue;
        if (CECS_UNLIKELY((mask & query->exclude_mask) != 0u)) continue;
        query->entity.slot = slot;
        query->entity.generation = world->slots.generations[slot];
        return 1u;
    }
    query->entity = cecs_entity_null();
    return 0u;
}

void *cecs_query_get(cecs_query *query, cecs_component_id component) {
    if (query == NULL) return NULL;
    if (cecs_entity_is_null(query->entity)) return NULL;
    return cecs_get(query->world, query->entity, component);
}

void cecs_schedule_init(cecs_schedule *schedule) {
    if (schedule == NULL) return;
    memset(schedule, 0, sizeof(*schedule));
}

int cecs_schedule_add(cecs_schedule *schedule, cecs_system_fn fn, void *ctx) {
    if (schedule == NULL || fn == NULL) return CECS_ERR_INVALID;
    if (schedule->count >= CECS_MAX_SYSTEMS) return CECS_ERR_FULL;
    schedule->systems[schedule->count].fn = fn;
    schedule->systems[schedule->count].ctx = ctx;
    schedule->systems[schedule->count].enabled = 1u;
    schedule->count++;
    return CECS_OK;
}

uint8_t cecs_schedule_count(const cecs_schedule *schedule) {
    if (schedule == NULL) return 0u;
    return schedule->count;
}

int cecs_schedule_enable(cecs_schedule *schedule, uint8_t index, uint8_t enabled) {
    if (schedule == NULL) return CECS_ERR_INVALID;
    if (index >= schedule->count) return CECS_ERR_INVALID;
    schedule->systems[index].enabled = (uint8_t)(enabled != 0u);
    return CECS_OK;
}

uint8_t cecs_schedule_is_enabled(const cecs_schedule *schedule, uint8_t index) {
    if (schedule == NULL) return 0u;
    if (index >= schedule->count) return 0u;
    return schedule->systems[index].enabled;
}

uint8_t cecs_schedule_run(cecs_schedule *schedule, cecs_world *world) {
    uint8_t i;
    uint8_t ran;
    if (schedule == NULL || world == NULL) return 0u;
    ran = 0u;
    for (i = 0u; i < schedule->count; i++) {
        if (schedule->systems[i].enabled) {
            schedule->systems[i].fn(world, schedule->systems[i].ctx);
            ran++;
        }
    }
    return ran;
}

#endif /* CECS_IMPLEMENTATION_ONCE */
#endif /* CECS_IMPLEMENTATION */
