#include <entityController.h>
#include <entityUtils.h>
#include <curve.h>
#include <events.h>

static size_t createEntity(struct game *const game, struct component *const geometry, struct component *const material, const char *const name, vec3s position, float rotation) {
        struct scene *const scene = game_getCurrentScene(game);
        struct object *object = scene_createObject(scene, name, 0);
        
        object_setComponent(object, geometry);
        object_setComponent(object, material);
        
        struct transform *transform = object_getComponent(object, COMPONENT_TRANSFORM);
        transform_reset(transform);
        transform_translate(transform, position);
        transform_rotateZ(transform, rotation);

        return object->idx;
}

static void onNetworkEntityNew(void *registerArgs, void *fireArgs) {
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityNew *args = fireArgs;

        struct networkEntity *entity = &controller->entities[args->idx];
        if (entity->init) {
                fprintf(stderr, "NETWORK NEW ENTITY ALREADY EXISTS\n");
                return;
        }

        entity->networkIdx = args->idx;

        struct scene *scene = game_getCurrentScene(controller->game);
        if (scene != NULL) {
                static char name[256];
                snprintf(name, 256, "networkEntity%lu", args->idx);
        
                struct object *player = scene_getObjectFromIdx(scene, controller->playerIdx);
                struct component *geometry = object_getComponent(player, COMPONENT_GEOMETRY);
                struct component *material = object_getComponent(player, COMPONENT_MATERIAL);
                entity->localIdx = createEntity(controller->game, geometry, material, name, args->position, args->rotation);
        }

        entity->prevPos = args->position;
        entity->nextPos = args->position;
        
        entity->prevRot = args->rotation;
        entity->nextRot = args->rotation;

        entity->lastUpdate = monotonic();
        
        entity->init = true;

        controller->numEntities++;
}

static void onNetworkEntityDel(void *registerArgs, void *fireArgs) {
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityDel *args = fireArgs;

        if (!controller->game->inScene) {
                return;
        }

        struct networkEntity *entity = &controller->entities[args->idx];
        if (!entity->init) {
                fprintf(stderr, "NETWORK DEL ENTITY DOES NOT EXIST\n");
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        if (scene != NULL) {
                struct object *object = scene_getObjectFromIdx(scene, entity->localIdx);
                scene_removeObject(scene, object);
        }
        
        entity->init = false;
        controller->numEntities--;
}

static void onNetworkEntityUpdate(void *registerArgs, void *fireArgs) {
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityUpdate *args = fireArgs;

        if (!controller->game->inScene) {
                return;
        }

        struct networkEntity *entity = &controller->entities[args->idx];
        if (!entity->init) {
                fprintf(stderr, "NETWORK UPDATE ENTITY DOES NOT EXIST\n");
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        if (scene != NULL) {
                struct object *object = scene_getObjectFromIdx(scene, entity->localIdx);
                struct transform *transform = object_getComponent(object, COMPONENT_TRANSFORM);

                vec4s t;
                mat4s r;
                vec3s s;
                glms_decompose(transform->model, &t, &r, &s);
                vec3s rr = glms_euler_angles(r);

                entity->prevPos = glms_vec3(t);
                entity->prevRot = rr.z;
        } else {
                entity->prevPos = entity->nextPos;
                entity->prevRot = entity->nextRot;
        }
        
        entity->nextPos = args->position;
        entity->nextRot = args->rotation;
        entity->lastUpdate = monotonic();
}

// angle lerp adapted from https://gist.github.com/shaunlebron/8832585
static float lerp_angle(float from, float to, float t) {
        float max = 2*GLM_PIf;
        float da = fmodf(to-from, max);
        float short_angle_dist = fmodf(2*da, max) - da;
        return from + short_angle_dist*t;
}

static void onUpdate(void *registerArgs, void *fireArgs) {
        struct entityController *controller = registerArgs;
        (void)fireArgs;

        if (!controller->game->inScene) {
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        if (scene == NULL) {
                return;
        }
        
        struct timespec now = monotonic();

        size_t count = 0;
        for (size_t i=0; i<MAX_ENTITIES && count<controller->numEntities; i++) {
                struct networkEntity *entity = &controller->entities[i];
                if (entity->init) {
                        unsigned long elapsed_ns = monotonic_difference(now, entity->lastUpdate);
                        float t = (float)elapsed_ns/(float)TICK_PERIOD_NS;
                        if (t > 2 || t < 0) {
                                continue;
                        }
                        
                        vec3s currPos;
                        float currRot;
                        if (t <= 1) {
                                currPos.x = glm_lerp(entity->prevPos.x, entity->nextPos.x, t);
                                currPos.y = glm_lerp(entity->prevPos.y, entity->nextPos.y, t);
                                currPos.z = glm_lerp(entity->prevPos.z, entity->nextPos.z, t);
                                currRot = lerp_angle(entity->prevRot, entity->nextRot, t);
                        } else {
                                currPos = entity->nextPos;
                                currRot = entity->nextRot;
                        }
                        
                        struct object *object = scene_getObjectFromIdx(scene, entity->localIdx);
                        struct transform *transform = object_getComponent(object, COMPONENT_TRANSFORM);
                        transform_reset(transform);
                        transform_translate(transform, currPos);
                        transform_rotateZ(transform, currRot);

                        count++;
                }
        }
}

static void onSceneChange(void *registerArgs, void *fireArgs) {
        struct entityController *controller = registerArgs;
        (void)fireArgs;

        if (!controller->game->inScene) {
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        controller->playerIdx = scene_idxByName(scene, controller->playerName);

        size_t count = 0;
        for (size_t i=0; i<MAX_ENTITIES && count<controller->numEntities; i++) {
                struct networkEntity *entity = &controller->entities[i];
                if (entity->init) {
                        static char name[256];
                        snprintf(name, 256, "networkEntity%lu", entity->networkIdx);
                        struct object *player = scene_getObjectFromIdx(scene, controller->playerIdx);
                        struct component *geometry = object_getComponent(player, COMPONENT_GEOMETRY);
                        struct component *material = object_getComponent(player, COMPONENT_MATERIAL);
                        entity->localIdx = createEntity(controller->game, geometry, material, name, entity->prevPos, entity->prevRot);
                }
        }
}

void entityController_setup(struct entityController *const controller, struct game *const game, const char *const playerName) {
        controller->game = game;
        controller->numEntities = 0;
        controller->playerName = playerName;

        for (size_t i=0; i<MAX_ENTITIES; i++) {
                controller->entities[i].init = false;
        }

        eventBroker_register(onNetworkEntityNew, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_NEW, controller);
        eventBroker_register(onNetworkEntityDel, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_DEL, controller);
        eventBroker_register(onNetworkEntityUpdate, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_UPDATE, controller);
        eventBroker_register(onSceneChange, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_SCENE_CHANGED, controller);

        eventBroker_register(onUpdate, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_UPDATE, controller);
}
