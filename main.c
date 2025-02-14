#include <raylib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define SCALE_FACTOR 32 // Integer scaling
#define FONTSET_SIZE 80
#define FONT_START_ADDRESS 0x80
#define START_ADDRESS 0x200

typedef struct Chip8_t {
	uint8_t ram[4096];
	uint8_t display[CHIP8_HEIGHT][CHIP8_WIDTH];
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
	// Default values
	int scaleFactor = 10;
	char *filename;
  // Parse command line arguments
	if (argc < 1 || strcmp(argv[1], "--help") == 0) {
		printf("Usage: %s --scale SCALE_FACTOR filename\nScale factor default is 10\n", argv[0]);
	}
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--scale") == 0) {
			if (i + 1 < argc) {
				scaleFactor = atoi(argv[i + 1]);
					if (scaleFactor <= 0) {
						fprintf(stderr, "Invalid scale factor. Using default (10).\n");
						scaleFactor = SCALE_FACTOR;
					}
				++i; // Skip the scale value
			} else {
				fprintf(stderr, "--scale requires a value.\n");
			}
		} else {
		// Assume it's the file path
		filename = argv[i];
		}
	}
	
	//***open file***
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
		fclose(file);
		return 1;
	}
	// window init
	const int windowWidth = CHIP8_WIDTH * scaleFactor;
	const int windowHeight = CHIP8_HEIGHT * scaleFactor;
	InitWindow(windowWidth, windowHeight, "CHIP-8-emu");
	
	//stack allocation for chip8
	chip8 chip;
	chip.pc = START_ADDRESS;
	// *** Keypad settings ***
	/* In the original COSMAC VIP, the keypad was set up as a HEX keypad like this:
	 
		1	2	3	C
		4	5	6	D
		7	8	9	E
		A	0	B	F

		in order to emulate they keypad i'm assigning keyboard keys to the HEX values like this:

		1	2	3	4
		Q	W	E	R
		A	S	D	F
		Z	X	C	V

		*/
	uint16_t keypad[16] = {KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, 
									KEY_Q, KEY_W, KEY_E, KEY_A,
									 KEY_S, KEY_D, KEY_Z, KEY_C, 
									  KEY_FOUR, KEY_R, KEY_F, KEY_V};
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
	free(buffer);

	Image screen_image = {
		.data = &chip.display,
		.width = CHIP8_WIDTH,
		.height = CHIP8_HEIGHT,
		.mipmaps = 1,
		.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
	};	
	Texture screen_texture = LoadTextureFromImage(screen_image);
	RenderTexture2D target = LoadRenderTexture(CHIP8_WIDTH, CHIP8_HEIGHT);

	struct timespec last_time;
	while (!WindowShouldClose()) {
		//Load opcode and increment PC to next instruction
		//since PC points to a single byte of ram, we bitshift to the left by 8 bits and OR it with the next 8 bits to get the full 12bit opcode
		double wait = 0;
		struct timespec cycle_start;
		struct timespec cycle_end;
		clock_gettime(CLOCK_MONOTONIC_RAW, &cycle_start);
		if (cycle_start.tv_sec > last_time.tv_sec && chip.timer_delay > 0) {
			chip.timer_delay--;
		}
		chip.opcode = (chip.ram[chip.pc] << 8u) | chip.ram[chip.pc+1];
		chip.pc += 2;
		// we will also need a random number every cycle
		uint8_t random = GetRandomValue(0, 255);
		//*** Emulate each opcode ***
		switch (chip.opcode & 0xF000u) {
			case 0x1000u: 
				//this is the JUMP instruction. Jump to the address in the last 3 digits in HEX
				chip.pc = chip.opcode & 0x0FFFu;
				wait = 0.000105;
				break;
			case 0x2000u:
				//This is the CALL instruction. Jump to the address indicated and also add a stack frame with a pointer to the previous instruction
				chip.pc = chip.opcode & 0x0FFFu;
				chip.stack[chip.idx_stack] = chip.pc;
				chip.idx_stack++;
				chip.pc = chip.opcode & 0x0FFFu;
				wait = 0.000105;
				break;
			case 0x3000u:
				//this is the SE Vx, byte instruction. It skips the next instruction if the value in the register specified by the second 4bits is equal to the value in the last 8 bits
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] == (chip.opcode & 0x00FFu))
					chip.pc += 2;
				wait = 0.000055;
				break;
			case 0x4000u:
				//this does the opposite of above. It skips if they DO NOT equal
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] != (chip.opcode & 0x00FFu))
					chip.pc +=2;
				wait = 0.000055;
				break;
			case 0x5000u:
				//skip if value at register indicated by second 4 bits is equal to value at register indicated by third 4 bits
				if (chip.registers[chip.opcode & 0x0F00u >> 8u] == chip.registers[chip.opcode & 0x00F0u])
					chip.pc += 2;
				wait = 0.000073;
				break;
			case 0x6000u:
				//put the value in the last two bytes into the register indicated by the second 4 bits
				chip.registers[chip.opcode & 0x0F00u] = (chip.opcode & 0x00FFu);
				wait = 0.000027;
				break;
			case 0x7000u:
				//add the value in the last two bytes to the value in the register indicated by the second 4 bits and store the sum in that register
				chip.registers[chip.opcode & 0x0F00u] += (chip.opcode & 0x00FFu);
				wait = 0.000045;
				break;
			case 0x8000u:
				switch (chip.opcode & 0xFu) {
					// 8xy0
					// Set Vx = Vy
					case 0x0u:
						chip.registers[(chip.opcode & 0x0F00u) >> 8u] = chip.registers[(chip.opcode & 0x00F0u) >> 4u];
						wait = 0.000200;
						break;
					// 8xy1
					// Set Vx = Vx | Vy
					case 0x1u:
						chip.registers[(chip.opcode & 0x0F00u) >> 8u] |= chip.registers[(chip.opcode & 0x00F0u) >> 4u];
						wait = 0.000200;
						break;
					// 8xy2
					// Set Vx = Vx & Vy
					case 0x2u:
						chip.registers[(chip.opcode & 0x0F00u) >> 8u] &= chip.registers[(chip.opcode & 0x00F0u) >> 4u];
						wait = 0.000200;
						break;
					case 0x3u:
						chip.registers[(chip.opcode & 0x0F00u) >> 8u] ^= chip.registers[(chip.opcode & 0x00F0u) >> 4u];
						wait = 0.000200;
						break;
					case 0x4u:
						/*Set Vx = Vx + Vy, set VF = carry.

The values of Vx and Vy are added together. If the result is greater than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest 8 bits of the result are kept, and stored in Vx.

This is an ADD with an overflow flag. If the sum is greater than what can fit into a byte (255), register VF will be set to 1 as a flag.*/
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						uint8_t Vy = (chip.opcode & 0x00F0u) >> 4u;
						uint16_t sum = chip.registers[Vx] + chip.registers[Vy];
						if (sum > 255)
							chip.registers[0xF] = 1;
						else
							chip.registers[0xF] = 0;
						chip.registers[Vx] = sum & 0xFFu;
						}
						wait = 0.000200;
						break;
					case 0x5u:
						/* Set Vx = Vx - Vy, set VF = NOT borrow.

If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx. */
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						uint8_t Vy = (chip.opcode & 0x00F0u) >> 4u;
						if (chip.registers[Vx] > chip.registers[Vy])
							chip.registers[0xF] = 1;
						else	
							chip.registers[0xF] = 9;
						chip.registers[Vx] -= chip.registers[Vy];
						}
						wait = 0.000200;
						break;
					case 0x6u:
					/* Set Vx = Vx SHR 1.

If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0. Then Vx is divided by 2.

A right shift is performed (division by 2), and the least significant bit is saved in Register VF. */
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						chip.registers[0xF] = (chip.registers[Vx] & 0x1u);
						chip.registers[Vx] >>= 1;
						}
						wait = 0.000200;
						break;
					case 0x7u:
					/* Set Vx = Vy - Vx, set VF = NOT borrow.

If Vy > Vx, then VF is set to 1, otherwise 0. Then Vx is subtracted from Vy, and the results stored in Vx. */
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						uint8_t Vy = (chip.opcode & 0x00F0u) >> 4u;
						if (chip.registers[Vy] > chip.registers[Vx])
							chip.registers[0xF] = 1;
						else
							chip.registers[0xF] = 0;
						chip.registers[Vx] = chip.registers[Vy] - chip.registers[Vx];
						}
						wait = 0.000200;
						break;
					case 0xE:
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						chip.registers[0xF] = (chip.registers[Vx] & 0x80u) >> 7u;
						chip.registers[Vx] <<= 1;
						}
						wait = 0.000200;
						break;
				}
				break;

			case 0x9000u:
				//skip next instruction if register in second 4 bits does not equal register in third 4 bits
				if (chip.registers[chip.opcode & 0x0F00u] != chip.registers[chip.opcode & 0x00F0U])
					chip.pc += 2;
				wait = 0.00073;
				break;
			case 0xA000u:
				//set the index register to the value in the last 12bits
				chip.idx_reg = chip.opcode & 0x0FFFu;
				wait = 0.000055;
				break;
			case 0xB000u:
				//jump to the address indicated by the last 12 bits + the value in register 0
				chip.pc = chip.registers[0] + chip.opcode & 0x00FFFu;
				wait = 0.000105;
				break;
			case 0xC000u:
				//generate a random number in the range of 0-255 and then AND that with the last byte of the opcode, then store that number in the register indicated by the second 4 bits
				chip.registers[chip.opcode & 0x0F00u >> 8u] = (random & (chip.opcode & 0x000Fu));
				wait = 0.000164;
				break;
			case 0xD000u:
				//Dxyn
				//Draw sprite with length n-bytes starting at location determined by registers xy
				chip.registers[15] = 0;
				uint8_t x = chip.registers[chip.opcode & 0x00F00u] % 64; 
				uint8_t y = chip.registers[chip.opcode & 0x000F0u] % 32;
				for (uint rows = 0; rows < (chip.opcode & 0x000F); rows++) {
					for (uint8_t columns = 0; columns < 8; columns++) {
						if ((chip.display[y][x] == 0xFF) && ((chip.ram[chip.idx_reg+rows] << columns) >= 128)) {
							chip.display[y][x] = 0;
							chip.registers[15] = 1;
						}else if ((chip.display[y][x] == 0) && ((chip.ram[chip.idx_reg+rows] << columns) >= 128)) {
								chip.display[y][x] = 0xFF;
							}
						if (x >= CHIP8_WIDTH) break;
						x++;
					}
					y++;
					if (y > CHIP8_HEIGHT) break;
				}
				//wait = 0.022734; --This is the average op time for this operation but i think it feels too slow so i reduced it
				wait = 0.012734;
				break;
			case 0xE000u:
				switch (chip.opcode & 0xFF) {
					case 0x9Eu:
					// Skip next instruction if key with the value of Vx is pressed.
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							uint8_t key = chip.registers[Vx];
							if (IsKeyPressed(keypad[key]))
			  					chip.pc += 2;
						}
						wait = 0.000073;
						break;
					case 0xA1u:
					// Skip if key is not pressed
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							uint8_t key = chip.registers[Vx];
							if (!(IsKeyPressed(keypad[key])))
			  					chip.pc += 2;
						}
						wait = 0.000073;
						break;
				}
				break;
			case 0xF000u:
				switch (chip.opcode & 0xFF) {
					case 0x07u:
						//set Vx = delay timer
						chip.registers[(chip.opcode & 0x0F00u) >> 8u] = chip.timer_delay;
						wait = 0.000073;
						break;
					case 0x0Au:
				}
						
		clock_gettime(CLOCK_MONOTONIC_RAW, &cycle_end);
		last_time = cycle_end;
		WaitTime(wait - ((cycle_end.tv_sec - cycle_start.tv_sec) + (cycle_end.tv_nsec - cycle_start.tv_nsec) / 1000000000.0));
		}
	}


	return 0;

}
