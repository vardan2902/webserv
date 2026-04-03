# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I./src
ifeq ($(shell uname), Darwin)
	CXXFLAGS += -isysroot $(shell xcrun --sdk macosx --show-sdk-path)
endif

# Directories
SRC_DIR = src
OBJ_DIR = obj

# Sources
SRCS = $(shell find $(SRC_DIR) -name "*.cpp")

# Objects
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Executable
NAME = webserv

# Colors for nicer output
GREEN = \033[0;32m
RESET = \033[0m

# Default target
all: $(NAME)

# Build executable
$(NAME): $(OBJS)
	@echo "$(GREEN)Linking executable...$(RESET)"
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# Compile .cpp into .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "$(GREEN)Compiling $<...$(RESET)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean object files
clean:
	@echo "$(GREEN)Cleaning objects...$(RESET)"
	@rm -rf $(OBJ_DIR)

# Full clean (objects + executable)
fclean: clean
	@echo "$(GREEN)Removing executable...$(RESET)"
	@rm -f $(NAME)

# Rebuild everything
re: fclean all

# Docker targets
docker-build:
	docker build -t webserv .

docker-run:
	docker run --rm -it -p 8080:8080 webserv bash

# Phony targets
.PHONY: all clean fclean re docker-build docker-run
