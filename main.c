#include <SDL2/SDL.h>
#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define SCALE_FACTOR 10 // Integer scaling
#define FONTSET_SIZE 80
#define FONT_START_ADDRESS 0x80
#define START_ADDRESS 0x200

typedef struct Chip8_t {
//registers
	uint32_t display[CHIP8_WIDTH-1][CHIP8_HEIGHT-1];
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
	long file_size = ftell(file);
	fseek(file,0,SEEK_SET);
	if (file_size == -1) {
		fprintf(stderr, "Could not get file size\n");
		fclose(file);
		return 1;
	}
	
	//***Read file into a buffer***
	char* buffer = malloc(sizeof(file_size));
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
	//we don't need the file anymore
	fclose(file);
	
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
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};
	for (uint i = 0; i < FONTSET_SIZE; ++i) {
		chip.ram[FONT_START_ADDRESS + i] = fontset[i];
	}
	//***copy buffer into the chip-8 ram***
	for (ulong i = 0; i <file_size; ++i) {
		chip.ram[START_ADDRESS + i] = buffer[i];
	}
	//we don't need this one anymore either
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
	//END SDL init
}
