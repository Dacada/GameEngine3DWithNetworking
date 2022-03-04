#include <sceneController.h>
#include <events.h>
#include <thirty/game.h>
#include <thirty/eventBroker.h>

static void onServerConnectionSuccess(void *registerArgs, void *fireArgs) {
        struct sceneController *controller = registerArgs;
        struct eventServerConnectionSuccess *args = fireArgs;
        (void)args;

        game_setCurrentScene(controller->game, controller->testSceneIdx);
}

static void onDisconnected(void *registerArgs, void *fireArgs) {
        struct sceneController *controller = registerArgs;
        struct eventBrokerNetworkDisconnected *args = fireArgs;
        (void)args;

        game_setCurrentScene(controller->game, controller->game->mainMenuScene);
}

void sceneController_setup(struct sceneController *const controller, struct game *game, size_t testSceneIdx) {
        controller->game = game;
        controller->testSceneIdx = testSceneIdx;

        eventBroker_register(onServerConnectionSuccess, EVENT_BROKER_PRIORITY_HIGH, (enum eventBrokerEvent)EVENT_SERVER_CONNECTION_SUCCESS, controller);
        eventBroker_register(onDisconnected, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_NETWORK_DISCONNECTED, controller);
}
