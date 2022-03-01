#include <entityController.h>
#include <curve.h>
#include <events.h>

static size_t createEntity(struct game *const game, const char *const name, vec3s position, float rotation) {
        struct scene *const scene = game_getCurrentScene(game);
        struct object *object = scene_createObject(scene, name, 0);

        struct component *geo = componentCollection_compByIdx(componentCollection_idxByName("Cube", COMPONENT_GEOMETRY));
        struct component *mat = componentCollection_compByIdx(componentCollection_idxByName("PlayerCharacterMaterial", COMPONENT_MATERIAL));
        
        object_setComponent(object, geo);
        object_setComponent(object, mat);
        
        struct transform *transform = object_getComponent(object, COMPONENT_TRANSFORM);
        transform_translate(transform, position);
        transform_rotateZ(transform, rotation);

        return object->idx;
}

static void onNetworkEntityNew(void *registerArgs, void *fireArgs) {
        fprintf(stderr, "new\n");
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityNew *args = fireArgs;

        struct networkEntity *entity = &controller->entities[args->idx];
        if (entity->init) {
                fprintf(stderr, "NETWORK NEW ENTITY ALREADY EXISTS\n");
                return;
        }
        
        static char name[256];
        snprintf(name, 256, "networkEntity%lu", args->idx);
        
        entity->localIdx = createEntity(controller->game, name, args->position, args->rotation);
        entity->init = true;
}

static void onNetworkEntityDel(void *registerArgs, void *fireArgs) {
        fprintf(stderr, "del\n");
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityDel *args = fireArgs;

        struct networkEntity *entity = &controller->entities[args->idx];
        if (!entity->init) {
                fprintf(stderr, "NETWORK DEL ENTITY DOES NOT EXIST\n");
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        struct object *object = scene_getObjectFromIdx(scene, entity->localIdx);
        scene_removeObject(scene, object);
        entity->init = false;
}

static void onNetworkEntityUpdate(void *registerArgs, void *fireArgs) {
        fprintf(stderr, "update\n");
        struct entityController *controller = registerArgs;
        struct eventNetworkEntityUpdate *args = fireArgs;

        struct networkEntity *entity = &controller->entities[args->idx];
        if (!entity->init) {
                fprintf(stderr, "NETWORK UPDATE ENTITY DOES NOT EXIST\n");
                return;
        }

        struct scene *scene = game_getCurrentScene(controller->game);
        struct object *object = scene_getObjectFromIdx(scene, entity->localIdx);
        struct transform *transform = object_getComponent(object, COMPONENT_TRANSFORM);
        transform_reset(transform);
        transform_translate(transform, args->position);
        transform_rotateZ(transform, args->rotation);
}

void entityController_setup(struct entityController *const controller, struct game *const game) {
        controller->game = game;

        for (size_t i=0; i<MAX_ENTITIES; i++) {
                controller->entities[i].init = false;
        }

        eventBroker_register(onNetworkEntityNew, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_NEW, controller);
        eventBroker_register(onNetworkEntityDel, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_DEL, controller);
        eventBroker_register(onNetworkEntityUpdate, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_NETWORK_ENTITY_UPDATE, controller);
}