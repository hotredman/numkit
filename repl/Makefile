.PHONY: all clean serve wasm

EMCC = emcc
BUILD_DIR = build
WEB_DIR = web

# Добавь сюда свои .cpp файлы интерпретатора
INTERPRETER_SRCS = \
	# src/your_lexer.cpp \
	# src/your_parser.cpp \
	# src/your_evaluator.cpp

REPL_SRCS = src/repl_bindings.cpp

ALL_SRCS = $(REPL_SRCS) $(INTERPRETER_SRCS)

EMCC_FLAGS = \
	--bind \
	-std=c++17 \
	-fexceptions \
	-O2 \
	-s MODULARIZE=1 \
	-s EXPORT_NAME='createMatlabModule' \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s INITIAL_MEMORY=67108864 \
	-s EXPORTED_RUNTIME_METHODS="['FS']"

all: wasm

wasm: $(BUILD_DIR)/matlab_repl.js

$(BUILD_DIR)/matlab_repl.js: $(ALL_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(EMCC) $(ALL_SRCS) $(EMCC_FLAGS) -o $(BUILD_DIR)/matlab_repl.js
	@cp $(WEB_DIR)/* $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)

# Локальный сервер для тестирования
serve: wasm
	@echo "Open http://localhost:8080"
	@cd $(BUILD_DIR) && python3 -m http.server 8080
