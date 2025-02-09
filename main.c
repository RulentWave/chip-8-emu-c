#include <SDL2/SDL.h>
#include <stdbool.h>
#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define SCALE_FACTOR 32 // Integer scaling
#define FONTSET_SIZE 80
#define FONT_START_ADDRESS 0x80
#define START_ADDRESS 0x200

typedef struct Chip8_t {
//registers
	uint32_t display[CHIP8_HEIGHT][CHIP8_WIDTH];
	uint8_t ram[4095];
	uint16_t stack[15];
	uint8_t registers[15];
	uint16_t idx_reg;
	uint16_t pc;
	uint16_t opcode;
	uint8_t idx_stack;
	uint8_t timer_delay;
	uint8_t timer_sound;
} chip8;

int main (int argc, char* argv[]) {
	
	
	//***check number of arguments***
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}
	
	//***open file***
	const char *filename = argv[1];
	FILE* file = fopen(filename, "rb");

	if (!file) {
		fprintf(stderr, "File %s not found\n", filename);
		return 1;
	}
	
	//***get file size***
	fseek(file, 0, SEEK_END);
	size_t file_size = ftell(file);
	fseek(file,0,SEEK_SET);
	if (file_size == -1) {
		fprintf(stderr, "Could not get file size\n");
		fclose(file);
		return 1;
	}
	
	//***Read file into a buffer***
	char* buffer = malloc(file_size);
	if (!buffer) {
		fprintf(stderr,"Could not allocate memory\n");
		fclose(file);
		return 1;
	}
	size_t bytes_read = fread(buffer, 1, file_size, file);
	if (bytes_read != file_size) {
		if (ferror(file)) {
			fprintf(stderr, "error reading file\n");
		}else {
			fprintf(stderr, "short read: expected %ld bytes, got %zu\n", file_size, bytes_read);
		}
		if (file_size > (4096 - START_ADDRESS)) fprintf(stderr, "File too big to be a chip-8 ROM\n");
		free(buffer);
		fclose(file);
		return 1;
	}
	
	//stack allocation for chip8
	chip8 chip;
	chip.pc = START_ADDRESS;
	//font sprite magic numbers
	uint8_t fontset[FONTSET_SIZE] =
	{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80	// F
	};
	for (uint i = 0; i < FONTSET_SIZE; ++i) {
		chip.ram[FONT_START_ADDRESS + i] = fontset[i];
	}
	//***copy buffer into the chip-8 ram***
	for (ulong i = 0; i <file_size; ++i) {
		chip.ram[START_ADDRESS + i] = buffer[i];
	}
	//we don't need this anymore 
	fclose(file);
	free(buffer);


	//heap allocation for chip8
	/*chip8* chip = malloc(sizeof(chip8));
	if (!chip) {
		fprintf(stderr,"Could not allocate memory");
		return 1;
	}*/
	//stack allocation for chip8

	//SDL initialization
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
		return 1;
	}
		SDL_Window* window = SDL_CreateWindow(
		"Chip-8 Emulator",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		CHIP8_WIDTH * SCALE_FACTOR,
		CHIP8_HEIGHT * SCALE_FACTOR,
		SDL_WINDOW_SHOWN
	);

	if (!window) {
		fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
	//create the texture that we will render to
	SDL_Texture* texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		CHIP8_WIDTH,
		CHIP8_HEIGHT
	);

	//error handling
	if (!texture) {
		fprintf(stderr, "Texture creation failed: %s\n", SDL_GetError());
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
	//Checkerboard to test display
	for (uint y = 0; y < CHIP8_HEIGHT; ++y) {
		for (uint x = 0; x < CHIP8_WIDTH; ++x) {
			if ((x + y) % 2 == 0)
				chip.display[y][x] = 0xFFFFFFFF;  // White
			else
				chip.display[y][x] = 0x00000000;  // Black
		}
	}
	
	//***Main Event Loop***
	SDL_Event event;
	bool quit = false;

	while (!quit) {
		while (SDL_PollEvent(&event) !=0) {
			if (event.type == SDL_QUIT) {
				quit = true;
			}
		}
		//put display into texture
		SDL_UpdateTexture(texture,NULL, chip.display, CHIP8_WIDTH * sizeof(uint32_t));
		//clear the renderer
		SDL_SetRenderDrawColor(renderer, 0,0,0,255);
		SDL_RenderClear(renderer);

		//Render the texture to the window (scaled)
		SDL_Rect destRect = {0,0, CHIP8_WIDTH * SCALE_FACTOR, CHIP8_HEIGHT * SCALE_FACTOR};
		SDL_RenderCopy(renderer, texture, NULL, &destRect);

		//Update the screen
		SDL_RenderPresent(renderer);
		SDL_Delay(16);
	}

	//Clean up and quit
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;

}
