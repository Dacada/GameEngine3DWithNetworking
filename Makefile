MAKEFLAGS := --warn-undefined-variables

SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := obj
INCLUDE_DIR := include
ASSETS_DIR := assets

SOURCES := $(wildcard $(SRC_DIR)/*.c)
SOURCES_SERVER := $(SRC_DIR)/server.c
SOURCES := $(filter-out $(SOURCES_SERVER),$(SOURCES))
SOURCES_SERVER := $(SOURCES_SERVER) $(SRC_DIR)/curve.c $(SRC_DIR)/timeutil.c $(SRC_DIR)/entityUtils.c

OBJECTS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_dbg.o,$(SOURCES))
OBJECTS_RELEASE := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_rel.o,$(SOURCES))
OBJECTS_DEVELOP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_dev.o,$(SOURCES))

OBJECTS_SERVER_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_dbg.o,$(SOURCES_SERVER))
OBJECTS_SERVER_RELEASE := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_rel.o,$(SOURCES_SERVER))
OBJECTS_SERVER_DEVELOP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%_dev.o,$(SOURCES_SERVER))

DEPENDS_DEBUG := $(OBJECTS_DEBUG:.o=.d)
DEPENDS_RELEASE := $(OBJECTS_RELEASE:.o=.d)
DEPENDS_DEVELOP := $(OBJECTS_DEVELOP:.o=.d)

DEPENDS_SERVER_DEBUG := $(OBJECTS_SERVER_DEBUG:.o=.d)
DEPENDS_SERVER_RELEASE := $(OBJECTS_SERVER_RELEASE:.o=.d)
DEPENDS_SERVER_DEVELOP := $(OBJECTS_SERVER_DEVELOP:.o=.d)

TARGETS := $(BIN_DIR)/main_dbg $(BIN_DIR)/main_rel $(BIN_DIR)/main_dev $(BIN_DIR)/server_dbg $(BIN_DIR)/server_rel $(BIN_DIR)/server_dev

FONTS_FTD := $(ASSETS_DIR)/fonts/Cabin-Regular_36_latin-1.ftd
FONTS_FTD += $(ASSETS_DIR)/fonts/CutiveMono-Regular_24_latin-1.ftd
FONTS_PNG := $(patsubst $(ASSETS_DIR)/fonts/%.ftd,$(ASSETS_DIR)/textures/%.png,$(FONTS_FTD))


CC := gcc

CFLAGS := -I$(realpath $(INCLUDE_DIR)) -Ilib/thirty/include `pkg-config --cflags glfw3` `pkg-config --cflags cglm` `pkg-config --cflags libenet` -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wimplicit-fallthrough -Wstringop-overflow=4 -std=c11
LDFLAGS := `pkg-config --libs glfw3` `pkg-config --libs cglm` `pkg-config --libs libenet` -lm -ldl -std=c11

CFLAGS_SERVER := -I$(realpath $(INCLUDE_DIR)) -Ilib/thirty/include `pkg-config --cflags libenet` -Wall -Wextra -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wimplicit-fallthrough -Wstringop-overflow=4 -std=c11
LDFLAGS_SERVER := `pkg-config --libs libenet` -lm -ldl -std=c11

CFLAGS_DEBUG := -MMD -Og -g -fno-omit-frame-pointer
LDFLAGS_DEBUG := $(CFLAGS_DEBUG)
CFLAGS_RELEASE := -MMD -DNDEBUG -flto -O2 -g
LDFLAGS_RELEASE := $(CFLAGS_RELEASE)
LDFLAGS_DEVELOP := -fsanitize=address -fsanitize=undefined -Og -fno-omit-frame-pointer
CFLAGS_DEVELOP := -MMD $(LDFLAGS_DEVELOP) -Werror

define FIND_HEADERS_CMD
( \
    find . '(' -type d -name .git ')' -prune -o \
           '(' -type d -name venv ')' -prune -o \
           -type f '(' -name '*.c' -o -name '*.h' ')' -print; \
)
endef

define FIND_SYSHEADERS_CMD
( \
    find /usr/include -type f -name '*.h' -print; \
)
endef

.PHONY: dbg dev rel clearfonts clean veryclean purify impolute etags glad_rel glad_dbg fonts valgrind static-analysis

rel: stb_img glad_rel fonts $(BIN_DIR)/main $(BIN_DIR)/server
dev: stb_img glad_dbg fonts etags $(BIN_DIR)/main_dev $(BIN_DIR)/server_dev
dbg: stb_img glad_dbg fonts $(BIN_DIR)/main_dbg $(BIN_DIR)/server_dbg

glad_rel:
	make glad_rel -C lib/thirty
glad_dbg:
	make glad_dbg -C lib/thirty
stb_img:
	make include/stb_image.h -C lib/thirty

clearfonts:
	-rm -f $(FONTS_PNG)
	-rm -rf $(ASSETS_DIR)/fonts
clean: clearfonts
	-rm -f $(OBJ_DIR)/*.o
	-rm -rf $(BIN_DIR)
	make clean -C lib/thirty
veryclean: clean
	-rm -rf $(OBJ_DIR)
	-rm -f TAGS
	make veryclean -C lib/thirty
purify: clearfonts veryclean
	-rm -f sysh_TAGS
	make purify -C lib/thirty
impolute: purify
	-rm -rf venv
	-rm -rf lib/BitmapFontGenerator/venv
	make impolute -C lib/thirty

etags: sysh_TAGS
	-rm -f TAGS
	$(FIND_HEADERS_CMD) | etags --include=$< -

sysh_TAGS:
	$(FIND_SYSHEADERS_CMD) | etags -o $@ -

fonts: $(FONTS_FTD) $(FONTS_PNG)

valgrind: $(BIN_DIR)/main_dbg
	valgrind --leak-check=full           			\
                 --show-leak-kinds=definite,indirect,possible	\
	         --track-origins=yes         			\
	         --verbose                   			\
	         --log-file=valgrind-out.txt 			\
                 --keep-debuginfo=yes                           \
	         $(BIN_DIR)/main_dbg

lib/thirty/bin/thirty_dbg.a:
	make dbg -C lib/thirty
lib/thirty/bin/thirty.a:
	make rel -C lib/thirty
lib/thirty/bin/thirty_dev.a:
	make dev -C lib/thirty

$(BIN_DIR)/main_dbg: LDFLAGS += $(LDFLAGS_DEBUG)
$(BIN_DIR)/main_rel: LDFLAGS += $(LDFLAGS_RELEASE)
$(BIN_DIR)/main_dev: LDFLAGS += $(LDFLAGS_DEVELOP)

$(BIN_DIR)/main_dbg: $(OBJECTS_DEBUG) lib/thirty/bin/thirty_dbg.a
$(BIN_DIR)/main_rel: $(OBJECTS_RELEASE) lib/thirty/bin/thirty.a
$(BIN_DIR)/main_dev: $(OBJECTS_DEVELOP) lib/thirty/bin/thirty_dev.a

$(BIN_DIR)/server_dbg: LDFLAGS := $(LDFLAGS_SERVER) $(LDFLAGS_DEBUG)
$(BIN_DIR)/server_rel: LDFLAGS := $(LDFLAGS_SERVER) $(LDFLAGS_RELEASE)
$(BIN_DIR)/server_dev: LDFLAGS := $(LDFLAGS_SERVER) $(LDFLAGS_DEVELOP)

$(BIN_DIR)/server_dbg: $(OBJECTS_SERVER_DEBUG)
$(BIN_DIR)/server_rel: $(OBJECTS_SERVER_RELEASE)
$(BIN_DIR)/server_dev: $(OBJECTS_SERVER_DEVELOP)

$(OBJECTS_SERVER_DEBUG): CFLAGS := $(CFLAGS_SERVER)
$(OBJECTS_SERVER_RELEASE): CFLAGS := $(CFLAGS_SERVER)
$(OBJECTS_SERVER_DEVELOP): CFLAGS := $(CFLAGS_SERVER)

$(OBJ_DIR)/%_dbg.o: CFLAGS += $(CFLAGS_DEBUG)
$(OBJ_DIR)/%_rel.o: CFLAGS += $(CFLAGS_RELEASE)
$(OBJ_DIR)/%_dev.o: CFLAGS += $(CFLAGS_DEVELOP)

$(OBJ_DIR)/%_dbg.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<
$(OBJ_DIR)/%_rel.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<
$(OBJ_DIR)/%_dev.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGETS):
	mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@

$(BIN_DIR)/main: $(BIN_DIR)/main_rel
	cp $< $@

$(BIN_DIR)/server: $(BIN_DIR)/server_rel
	cp $< $@


$(SRC_DIR)/.clang_complete $(INCLUDE_DIR)/.clang_complete: Makefile
	echo $(CFLAGS) | tr " " "\n" > $@


$(ASSETS_DIR)/fonts/Cabin-Regular.ttf:
	mkdir -p $(ASSETS_DIR)/fonts
	wget -q -O $@ "https://raw.githubusercontent.com/impallari/Cabin/master/fonts/TTF/Cabin-Bold.ttf"
$(ASSETS_DIR)/fonts/CutiveMono-Regular.ttf:
	mkdir -p $(ASSETS_DIR)/fonts
	wget -q -O $@ "https://raw.githubusercontent.com/vernnobile/CutiveFont/master/CutiveMono/GWF-1.001/CutiveMono-Regular.ttf"

$(ASSETS_DIR)/textures/Cabin-Regular_36_latin-1.png $(ASSETS_DIR)/fonts/Cabin-Regular_36_latin-1.ftd &: $(ASSETS_DIR)/fonts/Cabin-Regular.ttf lib/BitmapFontGenerator/venv
	set -e; \
	source lib/BitmapFontGenerator/venv/bin/activate; \
	python lib/BitmapFontGenerator/BitmapFontGenerator.py --basedir $(ASSETS_DIR) --height 36 --encoding latin-1 $<
$(ASSETS_DIR)/textures/CutiveMono-Regular_24_latin-1.png $(ASSETS_DIR)/fonts/CutiveMono-Regular_24_latin-1.ftd &: $(ASSETS_DIR)/fonts/CutiveMono-Regular.ttf lib/BitmapFontGenerator/venv
	set -e; \
	source lib/BitmapFontGenerator/venv/bin/activate; \
	python lib/BitmapFontGenerator/BitmapFontGenerator.py --basedir $(ASSETS_DIR) --height 24 --encoding latin-1 $<

lib/BitmapFontGenerator/venv: lib/BitmapFontGenerator/requirements.txt
	virtualenv lib/BitmapFontGenerator/venv
	source lib/BitmapFontGenerator/venv/bin/activate; \
	pip install -r lib/BitmapFontGenerator/requirements.txt


-include $(DEPENDS_DEBUG)
-include $(DEPENDS_RELEASE)
-include $(DEPENDS_DEVELOP)
-include $(DEPENDS_SERVER_DEBUG)
-include $(DEPENDS_SERVER_RELEASE)
-include $(DEPENDS_SERVER_DEVELOP)
