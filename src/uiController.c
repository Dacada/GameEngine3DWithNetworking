#include <uiController.h>
#include <string.h>

#define FRAME_PERIOD_FPS_REFRESH 10

#define UI_STATUS_WINDOW_WIDTH 100.0f
#define UI_STATUS_WINDOW_HEIGHT 30.0f

#define UI_SERVER_SELECT_WINDOW_WIDTH 300.0f
#define UI_SERVER_SELECT_WINDOW_HEIGHT 146.0f

#define UI_SERVER_SELECT_STATUS_WINDOW_WIDTH 250.0f
#define UI_SERVER_SELECT_STATUS_WINDOW_HEIGHT 115.0f

static void updateFps(struct uiControllerStatusData *data, float delta) {
        data->fpsChanged = false;
        data->deltas += delta;
        
        data->count++;
        if (data->count >= FRAME_PERIOD_FPS_REFRESH) {
                float avgDelta = data->deltas/FRAME_PERIOD_FPS_REFRESH;
                data->fps = (unsigned)(1/avgDelta);
                if (data->fps != data->prevFps) {
                        data->fpsChanged = true;
                        data->prevFps = data->fps;
                }
                
                data->deltas = 0;
                data->count = 0;
        }
}

static void updatePing(struct uiControllerStatusData *data, ENetPeer *server) {
        data->pingChanged = false;
        if (server == NULL) {
                return;
        }
        data->ping = server->roundTripTime;
        
        if (data->ping != data->prevPing) {
                data->pingChanged = true;
                data->prevPing = data->ping;
        }
}

////////////////////////////////////////////////////////////////////////////////

static void doConnect(struct uiControllerServerSelectData *data,
                      struct networkController *networkController) {
        if (data->hostBuffer[0] == '\0' ||
            data->portBuffer[0] == '\0') {
                data->connectionStatus = UI_SERVER_SELECT_STATUS_ERROR;
                data->errorMsg = "Both fields must be filled.";
        } else {
                networkController_connect(networkController, data->hostBuffer,
                                          (unsigned short)atoi(data->portBuffer));
                data->connectionStatus = UI_SERVER_SELECT_STATUS_CONNECTING;
        }
}

static void updateUI_serverSelectWidget_inputWindow(
        struct uiControllerServerSelectData *data, struct nk_context *ctx,
        struct networkController *networkController, struct game *game,
        int width, int height) {

        struct nk_rect bounds = nk_rect(
                (float)width/2-UI_SERVER_SELECT_WINDOW_WIDTH/2,
                (float)height/2-UI_SERVER_SELECT_WINDOW_HEIGHT/2,
                UI_SERVER_SELECT_WINDOW_WIDTH, UI_SERVER_SELECT_WINDOW_HEIGHT);
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE;
        
        if (nk_begin(ctx, "Server Selection", bounds, flags)) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 2);
                nk_layout_row_push(ctx, 0.25f);
                nk_label(ctx, "Host:", NK_TEXT_RIGHT);
                nk_layout_row_push(ctx, 0.65f);
                if (data->shouldFocusCurrentEdit && data->currentEdit == 0) {
                        nk_edit_focus(ctx, NK_EDIT_DEFAULT);
                        data->shouldFocusCurrentEdit = false;
                }
                nk_edit_string_zero_terminated(
                        ctx, NK_EDIT_FIELD, data->hostBuffer, 256, nk_filter_ascii);
                
                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 2);
                nk_layout_row_push(ctx, 0.25f);
                nk_label(ctx, "Port:", NK_TEXT_RIGHT);
                nk_layout_row_push(ctx, 0.65f);
                if (data->shouldFocusCurrentEdit && data->currentEdit == 1) {
                        nk_edit_focus(ctx, NK_EDIT_DEFAULT);
                        data->shouldFocusCurrentEdit = false;
                }
                nk_edit_string_zero_terminated(
                        ctx, NK_EDIT_FIELD, data->portBuffer, 6, nk_filter_decimal);

                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 3);
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_RIGHT);
                nk_layout_row_push(ctx, 0.4f);
                if (nk_button_label(ctx, "Connect")) {
                        doConnect(data, networkController);
                }
                nk_layout_row_push(ctx, 0.4f);
                if (nk_button_label(ctx, "Exit")) {
                        game_shouldStop(game);
                }
        }
        nk_end(ctx);
}

static void updateUI_serverSelectWidget_connectingWindow(
        struct uiControllerServerSelectData *data, struct nk_context *ctx,
        struct networkController *networkController, int width, int height) {
        
        struct nk_rect bounds = nk_rect(
                (float)width/2-UI_SERVER_SELECT_STATUS_WINDOW_WIDTH/2,
                (float)height/2-UI_SERVER_SELECT_STATUS_WINDOW_HEIGHT/2,
                UI_SERVER_SELECT_STATUS_WINDOW_WIDTH,
                UI_SERVER_SELECT_STATUS_WINDOW_HEIGHT);
        
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE;
        
        if (nk_begin(ctx, "Connecting...", bounds, flags)) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 1);
                nk_layout_row_push(ctx, 1.0f);
                nk_label(ctx, "Connecting to server...", NK_TEXT_CENTERED);

                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 3);
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.8f);
                if (nk_button_label(ctx, "Cancel")) {
                        networkController_disconnect(networkController);
                        data->connectionStatus = UI_SERVER_SELECT_STATUS_ERROR;
                        data->errorMsg = "Connection cancelled.";
                }
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }
        nk_end(ctx);
}

static void updateUI_serverSelectWidget_errorWindow(
        struct uiControllerServerSelectData *data, struct nk_context *ctx,
        int width, int height) {
        
        struct nk_rect bounds = nk_rect(
                (float)width/2-UI_SERVER_SELECT_STATUS_WINDOW_WIDTH/2,
                (float)height/2-UI_SERVER_SELECT_STATUS_WINDOW_HEIGHT/2,
                UI_SERVER_SELECT_STATUS_WINDOW_WIDTH,
                UI_SERVER_SELECT_STATUS_WINDOW_HEIGHT);
        
        nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE;
        
        if (nk_begin(ctx, "Connection Error", bounds, flags)) {
                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 3);
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_CENTERED);
                nk_layout_row_push(ctx, 0.8f);
                nk_label(ctx, data->errorMsg, NK_TEXT_CENTERED);
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_CENTERED);

                nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 3);
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 0.8f);
                if (nk_button_label(ctx, "Accept")) {
                        data->connectionStatus = UI_SERVER_SELECT_STATUS_INPUT;
                }
                nk_layout_row_push(ctx, 0.1f);
                nk_label(ctx, "", NK_TEXT_LEFT);
        }
        nk_end(ctx);
}

////////////////////////////////////////////////////////////////////////////////

static void updateUI_serverSelectWidget(struct game *game, struct nk_context *ctx, int width, int height, struct networkController *networkController, struct uiControllerServerSelectData *data) {
        switch (data->connectionStatus) {
        case UI_SERVER_SELECT_STATUS_INPUT:
                updateUI_serverSelectWidget_inputWindow(
                        data, ctx, networkController, game, width, height);
                break;
        case UI_SERVER_SELECT_STATUS_CONNECTING:
                updateUI_serverSelectWidget_connectingWindow(
                        data, ctx, networkController, width, height);
                break;
        case UI_SERVER_SELECT_STATUS_ERROR:
                updateUI_serverSelectWidget_errorWindow(
                        data, ctx, width, height);
                break;
        case UI_SERVER_SELECT_STATUS_CONNECTED:
        default:
                break;
        }
}

static void updateUI_statusWidget(struct uiControllerStatusData *data, struct nk_context *ctx, struct game *game) {

        if (nk_begin(ctx, "status", nk_rect(0, 0, UI_STATUS_WINDOW_WIDTH, UI_STATUS_WINDOW_HEIGHT), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT)) {
                
                updateFps(data, ctx->delta_time_seconds);
                if (data->fpsChanged) {
                        snprintf(data->fpsBuffer, UI_FPS_BUFFER_SIZE, "%u", data->fps);
                }
                
                updatePing(data, game->server);
                if (data->pingChanged) {
                        snprintf(data->pingBuffer, UI_PING_BUFFER_SIZE, "%u", data->ping);
                }
                
                nk_layout_row_begin(ctx, NK_DYNAMIC, 10, 2);
                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, "fps: ", NK_TEXT_RIGHT);
                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, data->fpsBuffer, NK_TEXT_LEFT);
                
                nk_layout_row_begin(ctx, NK_DYNAMIC, 10, 2);
                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, "ping: ", NK_TEXT_RIGHT);
                nk_layout_row_push(ctx, 0.5f);
                nk_label(ctx, data->pingBuffer, NK_TEXT_LEFT);
        }
        nk_end(ctx);
}

////////////////////////////////////////////////////////////////////////////////

static void keyboardEvent(void *registerArgs, void *fireArgs) {
        struct uiController *controller = registerArgs;
        struct eventBrokerKeyboardEvent *args = fireArgs;

        if (args->action != GLFW_PRESS) {
                return;
        }

        if (args->key == GLFW_KEY_TAB) {
                if (game_inMainMenu(controller->game) && controller->serverSelectWidgetData.connectionStatus == UI_SERVER_SELECT_STATUS_INPUT) {
                        if (args->modifiers == GLFW_MOD_SHIFT) {
                                controller->serverSelectWidgetData.currentEdit--;
                        } else {
                                controller->serverSelectWidgetData.currentEdit++;
                        }
                        controller->serverSelectWidgetData.currentEdit %= 2;
                        controller->serverSelectWidgetData.shouldFocusCurrentEdit = true;
                }
        } else if (args->key == GLFW_KEY_ENTER) {
                if (controller->serverSelectWidgetData.connectionStatus == UI_SERVER_SELECT_STATUS_INPUT) {
                        doConnect(&controller->serverSelectWidgetData,
                                  controller->networkController);
                } else if (controller->serverSelectWidgetData.connectionStatus == UI_SERVER_SELECT_STATUS_ERROR) {
                        controller->serverSelectWidgetData.connectionStatus = UI_SERVER_SELECT_STATUS_INPUT;
                }
        } else if (args->key == GLFW_KEY_ESCAPE) {
                if (game_inMainMenu(controller->game)) {
                        game_shouldStop(controller->game);
                } else {
                        game_disconnect(controller->game, 0);
                }
        }
}

static void updateUI(void *registerArgs, void *fireArgs) {
        struct uiController *controller = registerArgs;
        struct eventBrokerUpdateUI *args = fireArgs;

        if (game_inMainMenu(controller->game)) {
                updateUI_serverSelectWidget(
                        controller->game, args->ctx, args->winWidth, args->winHeight,
                        controller->networkController, &controller->serverSelectWidgetData);
        } else {
                updateUI_statusWidget(&controller->statusWidgetData, args->ctx,
                                      controller->game);
        }
}

static void sceneChanged(void *registerArgs, void *fireArgs) {
        struct uiController *controller = registerArgs;
        struct eventBrokerSceneChanged *args = fireArgs;
        (void)args;

        if (game_inMainMenu(controller->game)) {
                controller->serverSelectWidgetData.currentEdit = 0;
                controller->serverSelectWidgetData.shouldFocusCurrentEdit = true;
        }
}

static void onConnect(void *registerArgs, void *fireArgs) {
        struct uiController *controller = registerArgs;
        struct eventBrokerNetworkConnected *args = fireArgs;
        (void)args;

        controller->serverSelectWidgetData.connectionStatus = UI_SERVER_SELECT_STATUS_CONNECTED;
}

static void onDisconnect(void *registerArgs, void *fireArgs) {
        struct uiController *controller = registerArgs;
        struct eventBrokerNetworkDisconnected *args = fireArgs;
        (void)args;

        switch (controller->serverSelectWidgetData.connectionStatus) {
        case UI_SERVER_SELECT_STATUS_INPUT:
        case UI_SERVER_SELECT_STATUS_CONNECTED:
                controller->serverSelectWidgetData.connectionStatus = UI_SERVER_SELECT_STATUS_ERROR;
                controller->serverSelectWidgetData.errorMsg = "Disconnected.";
                break;
        case UI_SERVER_SELECT_STATUS_CONNECTING:
                controller->serverSelectWidgetData.connectionStatus = UI_SERVER_SELECT_STATUS_INPUT;
                break;
        case UI_SERVER_SELECT_STATUS_ERROR:
        default:
                break;
        }
}

////////////////////////////////////////////////////////////////////////////////

void uiController_setup(struct uiController *controller, struct game *game, struct networkController *networkController) {
        controller->game = game;
        controller->networkController = networkController;

        controller->serverSelectWidgetData.hostBuffer[0] = '\0';
        controller->serverSelectWidgetData.portBuffer[0] = '\0';
        controller->serverSelectWidgetData.currentEdit = 0;
        controller->serverSelectWidgetData.shouldFocusCurrentEdit = true;
        controller->serverSelectWidgetData.connectionStatus = UI_SERVER_SELECT_STATUS_INPUT;
        controller->serverSelectWidgetData.errorMsg = "";

        controller->statusWidgetData.fps = 60;
        controller->statusWidgetData.prevFps = 60;
        controller->statusWidgetData.fpsChanged = false;
        strcpy(controller->statusWidgetData.fpsBuffer, "60");

        controller->statusWidgetData.ping = 500;
        controller->statusWidgetData.prevPing = 500;
        controller->statusWidgetData.pingChanged = false;
        strcpy(controller->statusWidgetData.fpsBuffer, "500");
        
        controller->statusWidgetData.deltas = 0;
        controller->statusWidgetData.count = 0;
        
        eventBroker_register(updateUI, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_UPDATE_UI, controller);
        eventBroker_register(keyboardEvent, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_KEYBOARD_EVENT, controller);
        eventBroker_register(sceneChanged, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_SCENE_CHANGED, controller);
        eventBroker_register(onConnect, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_NETWORK_CONNECTED, controller);
        eventBroker_register(onDisconnect, EVENT_BROKER_PRIORITY_HIGH, EVENT_BROKER_NETWORK_DISCONNECTED, controller);
}
