/*
Copyright (C) 2025 Eric Hernandez
See end of file for extended copyright information */

#include <raylib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <argp.h>

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


struct arguments {
	char* filename;
	long scale_factor;
	float fps;
	float hz;
};

static struct argp_option options[] = {
	{"scalefactor", 's', "NUMBER", 0, "Scaling factor. Defaults to 32", 0},
	{"fps", 'f', "NUMBER", 0, "FPS limit. Defaults to 60", 0},
	{"cpuherz", 'h', "NUMBER", 0, "Set clock speed in hz. By default, uses per instruction cycle speed that aproximates the original COSMIC VIP CHIP-8 timings", 0},
	{0}
};

static char doc[] = "Chip-8 Emulator";
static char args_doc[] = "chip-8-emu FILEPATH";


static error_t parse_opt (int key, char* arg, struct argp_state* state) {
	struct arguments* arguments = state->input;
	switch (key) {
		case 's':
			arguments->scale_factor = atoi(arg);
			break;
		case 'f':
			arguments->fps = atoi(arg);
			break;
		case 'h':
			arguments->hz = atoi(arg);
			break;
		case ARGP_KEY_ARG:
			if (state->arg_num >= 1)
				argp_usage(state);
			arguments->filename = arg;
			break;
		case ARGP_KEY_END:
			if (state->arg_num < 1)
				argp_usage(state);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}
static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc,
	.children = 0,
	.help_filter = 0,
	.argp_domain = NULL
};

int main (int argc, char* argv[]) {
  // Parse command line arguments
	struct arguments arguments;
	arguments.scale_factor = SCALE_FACTOR;
	arguments.filename = NULL;
	arguments.fps = 60.0;
	arguments.hz = 0.0;
	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	//arguments.filename = "INVADERS";

	//***open file***
	FILE* file = fopen(arguments.filename, "rb");

	if (!file) {
		fprintf(stderr, "File %s not found\n", arguments.filename);
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
	uint8_t* buffer = malloc(file_size);
	if (!buffer) {
		fprintf(stderr,"Could not allocate memory\n");
		fclose(file);
		return 1;
	}
	long bytes_read = fread(buffer, 1, file_size, file);
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
	const int windowWidth = CHIP8_WIDTH * arguments.scale_factor;
	const int windowHeight = CHIP8_HEIGHT * arguments.scale_factor;
	InitWindow(windowWidth, windowHeight, "CHIP-8-emu");
	
	//stack allocation for chip8
	chip8 chip = {0};
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
	for (long i = 0; i <file_size; ++i) {
		chip.ram[START_ADDRESS + i] = buffer[i];
	}
	//we don't need this anymore 
	free(buffer);

	Image screen_image = {
		.data = &chip.display[0][0],
		.width = CHIP8_WIDTH,
		.height = CHIP8_HEIGHT,
		.mipmaps = 1,
		.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
	};	
	Texture screen_texture = LoadTextureFromImage(screen_image);
	RenderTexture2D target = LoadRenderTexture(CHIP8_WIDTH, CHIP8_HEIGHT);


	struct timespec last_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &last_time);
	struct timespec gfx_clock = last_time;
	struct timespec cycle_start;
	struct timespec cycle_end;
	while (!WindowShouldClose()) {
		//Load opcode and increment PC to next instruction
		//since PC points to a single byte of ram, we bitshift to the left by 8 bits and OR it with the next 8 bits to get the full 12bit opcode
		clock_gettime(CLOCK_MONOTONIC_RAW, &cycle_start);
		double wait = 0.002;
		double elapsed = (cycle_start.tv_sec - last_time.tv_sec) + (cycle_start.tv_nsec - last_time.tv_nsec) / 1e9;
		if (elapsed >= (1.0 /60.0) && chip.timer_delay > 0) {
			last_time = cycle_start;
			chip.timer_delay--;
		}
		chip.opcode = (chip.ram[chip.pc] << 8u) | chip.ram[chip.pc+1];
		//printf("chip opcode is %X\n", chip.opcode);
		chip.pc += 2;
		// we will also need a random number every cycle
		uint8_t random = GetRandomValue(0, 255);
		//*** Emulate each opcode ***
		switch (chip.opcode & 0xF000u) {
			case 0x0000u:
				switch (chip.opcode) {
					case 0x00E0u:
					//clear the display
						memset(chip.display, 0, sizeof(chip.display));
						wait = 0.000109;
						break;
					case 0x00EEu:
					// return from subroutine
						if (chip.idx_stack == 0) {
							fprintf(stderr, "stack underflow\n");
							return 1;
						}
					chip.idx_stack--;
					chip.pc = chip.stack[chip.idx_stack];
					wait = 0.000105;
					break;
					}
				break;
			case 0x1000u: 
				//this is the JUMP instruction. Jump to the address in the last 3 digits in HEX
				chip.pc = chip.opcode & 0x0FFFu;
				wait = 0.000105;
				break;
			case 0x2000u:
				//This is the CALL instruction. Jump to the address indicated and also add a stack frame with a pointer to the previous instruction
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
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] == chip.registers[(chip.opcode & 0x00F0u) >> 4u])
					chip.pc += 2;
				wait = 0.000073;
				break;
			case 0x6000u:
				//put the value in the last two bytes into the register indicated by the second 4 bits
				chip.registers[(chip.opcode & 0x0F00u) >> 8u] = (chip.opcode & 0x00FFu);
				wait = 0.000027;
				break;
			case 0x7000u:
				//add the value in the last two bytes to the value in the register indicated by the second 4 bits and store the sum in that register
				chip.registers[(chip.opcode & 0x0F00u) >> 8u] += (chip.opcode & 0x00FFu);
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
							chip.registers[0xF] = 0;
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
				if (chip.registers[(chip.opcode & 0x0F00u) >> 8u] != chip.registers[(chip.opcode & 0x00F0U) >> 4u])
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
				chip.pc = chip.registers[0] + (chip.opcode & 0x0FFFu);
				wait = 0.000105;
				break;
			case 0xC000u:
				//generate a random number in the range of 0-255 and then AND that with the last byte of the opcode, then store that number in the register indicated by the second 4 bits
				chip.registers[(chip.opcode & 0x0F00u) >> 8u] = (random & (chip.opcode & 0x00FFu));
				wait = 0.000164;
				break;
			case 0xD000u:
				//Dxyn
				//Draw sprite with length n-bytes starting at location determined by registers xy
				chip.registers[0xF] = 0;
				uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
				uint8_t Vy = (chip.opcode & 0x00F0u) >> 4u;
				uint8_t x = chip.registers[Vx] % CHIP8_WIDTH; 
				uint8_t y = chip.registers[Vy] % CHIP8_HEIGHT;
				uint8_t height = chip.opcode & 0x000F;

				for (uint row = 0; row < height; row++) {
					uint8_t sprite = chip.ram[chip.idx_reg + row];

					for (uint8_t column = 0; column < 8; column++) {
						if ((sprite & (0x80 >> column)) != 0) {
							uint8_t pixel_x = (x+column) % CHIP8_WIDTH;
							uint8_t pixel_y = (y+row) % CHIP8_HEIGHT;
							if (chip.display[pixel_y][pixel_x] == 255)
								chip.registers[0xF] = 1; //This represents a colision as the sprite was already on
							chip.display[pixel_y][pixel_x] = chip.display[pixel_y][pixel_x] ? 0:255;
						}
					}
				}
				wait = 0.001734;
				break;
			case 0xE000u:
				switch (chip.opcode & 0xFF) {
					case 0x9Eu:
					// Skip next instruction if key with the value of Vx is pressed.
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							uint8_t key = chip.registers[Vx];
							if (IsKeyDown(keypad[key]))
			  					chip.pc += 2;
						}
						wait = 0.000073;
						break;
					case 0xA1u:
					// Skip if key is not pressed
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							uint8_t key = chip.registers[Vx];
							if (!(IsKeyDown(keypad[key])))
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
					//Wait for a key press, store the value of the key in Vx.
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							bool keyFound = false;
							for (uint_fast8_t i = 0; i < 16; i++) {
								if (IsKeyDown(keypad[i])) {
									chip.registers[Vx] = i;
									keyFound = true;
									break;
								}
							}
							if (!keyFound)
								chip.pc -= 2;
						} wait = 0.0; break;
					case 0x15u:
						// Set delay timer = Vx.
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							chip.timer_delay = chip.registers[Vx];
							wait = 0.000045;
						} break;
					case 0x18:
						chip.timer_sound = chip.registers[(chip.opcode & 0x0F00u) >> 8u];
						wait = 0.000045;
						break;
					case 0x1Eu:
						// Set I = I + Vx
						chip.idx_reg += chip.registers[(chip.opcode & 0x0F00u) >> 8u];
						wait = 0.000086;
						break;
					case 0x29u:
						// Set I = location of sprite for digit Vx
						{
						uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
						uint8_t digit = chip.registers[Vx];
						chip.idx_reg = FONT_START_ADDRESS + (5 * digit);
						}
						wait = 0.000096;
						break;
					case 0x33u:
						/* Store BCD representation of Vx in memory locations I, I+1, and I+2.
						The interpreter takes the decimal value of Vx, and places the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2. */
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							uint8_t value = chip.registers[Vx];
							chip.ram[chip.idx_reg]     = value / 100;
							chip.ram[chip.idx_reg + 1] = (value / 10) % 10;
							chip.ram[chip.idx_reg + 2] = value % 10;
						} wait = 0.000927; break;
					case 0x55u:
						// Store registers V0 through Vx in memory starting at location I
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							for (uint8_t i = 0; i <= Vx; ++i) {
								chip.ram[chip.idx_reg +i] = chip.registers[i];
							}
						} wait = 0.000605; break;
					case 0x65u:
						// Read registers V0 through Vx from memory starting at location I
						{
							uint8_t Vx = (chip.opcode & 0x0F00u) >> 8u;
							
							for (uint_fast8_t i =0; i <= Vx; ++i) {
								chip.registers[i] = chip.ram[chip.idx_reg + i];
							}
						} wait = 0.000605; break;
				}
		}

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);

		double ft = (now.tv_sec - gfx_clock.tv_sec) + (now.tv_nsec - gfx_clock.tv_nsec) / 1e9;
		if (ft >= (1.0/arguments.fps)) {
		gfx_clock = now;
		UpdateTexture(screen_texture, &chip.display[0][0]);
		BeginDrawing();

		BeginTextureMode(target);
		DrawTexture(screen_texture, 0, 0, WHITE);
		EndTextureMode();

		DrawTexturePro(
			target.texture, (Rectangle){0,0, target.texture.width, -target.texture.height}, (Rectangle){0,0,windowWidth,windowHeight},
			(Vector2){0,0}, 0.0f, WHITE);
		EndDrawing();
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &cycle_end);
		if (arguments.hz)
			wait = (1.0/arguments.hz);
		WaitTime(wait - ((cycle_end.tv_sec - cycle_start.tv_sec) + (cycle_end.tv_nsec - cycle_start.tv_nsec) / 1e9));
//		WaitTime(wait);
		}
	//De-init
	UnloadRenderTexture(target);
	UnloadTexture(screen_texture);
	CloseWindow();
	return 0;
	}
/*MIT License
Copyright (c) 2025 Eric Hernandez

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
