#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <thirty/game.h>
#include <networkController.h>

#define UI_HOST_BUFFER_SIZE 256
#define UI_PORT_BUFFER_SIZE 6

#define UI_FPS_BUFFER_SIZE 6
#define UI_PING_BUFFER_SIZE 6

struct uiControllerStatusData {
        unsigned fps;
        unsigned prevFps;
        bool fpsChanged;
        char fpsBuffer[UI_FPS_BUFFER_SIZE];

        unsigned ping;
        unsigned prevPing;
        bool pingChanged;
        char pingBuffer[UI_PING_BUFFER_SIZE];
        
        float deltas;
        unsigned count;
};

struct uiControllerServerSelectData {
        char hostBuffer[UI_HOST_BUFFER_SIZE];
        char portBuffer[UI_PORT_BUFFER_SIZE];
        
        int currentEdit;
        bool shouldFocusCurrentEdit;
        
        size_t sceneLoadProgressCurrent;
        size_t sceneLoadProgressTotal;
        
        enum {
                UI_SERVER_SELECT_STATUS_INPUT,
                UI_SERVER_SELECT_STATUS_CONNECTING,
                UI_SERVER_SELECT_STATUS_ERROR,
                UI_SERVER_SELECT_STATUS_CONNECTED,
        } connectionStatus;
        const char *errorMsg;
};

struct uiController {
        struct game *game;
        struct networkController *networkController;

        struct uiControllerStatusData statusWidgetData;
        struct uiControllerServerSelectData serverSelectWidgetData;
};

void uiController_setup(struct uiController *controller, struct game *game, struct networkController *networkController)
        __attribute__((access (read_only, 1)))
        __attribute__((access (read_write, 2)))
        __attribute__((access (read_only, 3)))
        __attribute__((nonnull));

#endif /* UI_CONTROLLER_H */
