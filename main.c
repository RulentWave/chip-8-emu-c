#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define SCALE_FACTOR 32 // Integer scaling
#define FONTSET_SIZE 80
#define FONT_START_ADDRESS 0x80
#define START_ADDRESS 0x200

typedef struct Chip8_t {
	uint32_t display[CHIP8_HEIGHT][CHIP8_WIDTH];
	uint8_t ram[4096];
	uint16_t stack[16];
	uint8_t registers[16];
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
	for (uint y = 0; y < CHIP8_HEIGHT; y++) {
		for (uint x = 0; x < CHIP8_WIDTH; x++) {
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
		//Load opcode and increment PC to next instruction
		//since PC points to a single byte of ram, we bitshift to the left by 8 bits and OR it with the next 8 bits to get the full 12bit opcode
		chip.opcode = (chip.ram[chip.pc] << 8u) | chip.ram[chip.pc+1];
		chip.pc += 2;
		// we will also need a random number every cycle
		srand(time(NULL));
		uint8_t random = rand();
		//*** Emulate each opcode ***
		switch (chip.opcode & 0xF0000u) {
			case 0x1000u: 
				//this is the JUMP instruction. Jump to the address in the last 3 digits in HEX
				chip.pc = chip.opcode & 0x0FFFu;
				break;
			case 0x2000u:
				//This is the CALL instruction. Jump to the address indicated and also add a stack frame with a pointer to the previous instruction
				chip.pc = chip.opcode & 0x0FFFu;
				chip.stack[chip.idx_stack] = chip.pc;
				chip.idx_stack++;
				chip.pc = chip.opcode & 0x0FFFu;
				break;
			case 0x3000u:
				//this is the SE Vx, byte instruction. It skips the next instruction if the value in the register specified by the second 4bits is equal to the value in the last 8 bits
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] == (chip.opcode & 0x00FFu))
					chip.pc += 2;
				break;
			case 0x4000u:
				//this does the opposite of above. It skips if they DO NOT equal
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] != (chip.opcode & 0x00FFu))
					chip.pc +=2;
				break;
			case 0x5000u:
				//skip if value at register indicated by second 4 bits is equal to value at register indicated by third 4 bits
				if (chip.registers[chip.opcode & 0x0F00u >> 8u] == chip.registers[chip.opcode & 0x00F0u])
					chip.pc += 2;
				break;
			case 0x6000u:
				//put the value in the last two bytes into the register indicated by the second 4 bits
				chip.registers[chip.opcode & 0x0F00u] = (chip.opcode & 0x00FFu);
				break;
			case 0x7000u:
				//add the value in the last two bytes to the value in the register indicated by the second 4 bits and store the sum in that register
				chip.registers[chip.opcode & 0x0F00u] += (chip.opcode & 0x00FFu);
				break;
			case 0x9000u:
				//skip next instruction if register in second 4 bits does not equal register in third 4 bits
				if (chip.registers[chip.opcode & 0x0F00u] != chip.registers[chip.opcode & 0x00F0U])
					chip.pc += 2;
				break;
			case 0xA000u:
				//set the index register to the value in the last 12bits
				chip.idx_reg = chip.opcode & 0x0FFFu;
				break;
			case 0xB000u:
				//jump to the address indicated by the last 12 bits + the value in register 0
				chip.pc = chip.registers[0] + chip.opcode & 0x00FFFu;
				break;
			case 0xC000u:
				//generate a random number in the range of 0-255 and then AND that with the last byte of the opcode, then store that number in the register indicated by the second 4 bits
				chip.registers[chip.opcode & 0x0F00u >> 8u] = (random & (chip.opcode & 0x000Fu));
				break;
			case 0xD000u:
				//TODO Dxyn
				//Draw sprite with length n-bytes starting at location determined by registers xy



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
