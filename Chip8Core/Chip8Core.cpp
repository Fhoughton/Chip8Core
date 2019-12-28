#include <iostream>
#include <fstream>
#include <vector>

//Sleep imports
#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

//Memory storage
unsigned char graphics[2048]; //64 x 32 screen
unsigned char memory[4096];
unsigned char registers[16];
unsigned short indexregister = 0;
unsigned short programcounter = 0x200;
unsigned short opcode = 0; //Current instruction

//System timers
unsigned char delay_timer;
unsigned char sound_timer;

//Stack control for returning after a jump
unsigned short stack[16];
unsigned short stackpointer = 0;

//Store current key of user input
unsigned char key[16];

//Flags
bool draw = true;

//Buffer for file handling
const unsigned int buffersize = 4096;

const unsigned char fontset[80] =
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

//Cross platform sleep function
void sleep_ms(int milliseconds)
{
#ifdef WIN32
	Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
#else
	usleep(milliseconds * 1000);
#endif
}

//Console reference
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

int main(int argc, char **argv)
{
	//Clear display
	for (int i = 0; i < 2048; ++i)
		graphics[i] = 0;

	//Clear stack
	for (int i = 0; i < 16; ++i)
		stack[i] = 0;

	for (int i = 0; i < 16; ++i)
		key[i] = registers[i] = 0;

	//Clear memory
	for (int i = 0; i < 4096; ++i)
		memory[i] = 0;

	//Load the fontset
	for (int i = 0; i < 80; ++i)
		memory[i] = fontset[i];

	//Load the game into ram
	FILE * pFile = fopen(argv[1], "rb");
	if (pFile == NULL)
	{
		fputs("File error", stderr);
		return false;
	}
	fseek(pFile, 0, SEEK_END);
	long lSize = ftell(pFile);
	rewind(pFile);
	printf("Filesize: %d\n", (int)lSize);
	char * buffer = (char*)malloc(sizeof(char) * lSize);
	if (buffer == NULL)
	{
		fputs("Memory error", stderr);
		return false;
	}
	size_t result = fread(buffer, 1, lSize, pFile);
	if (result != lSize)
	{
		fputs("Reading error", stderr);
		return false;
	}
	if ((4096 - 512) > lSize)
	{
		for (int i = 0; i < lSize; ++i)
			memory[i + 512] = buffer[i];
	}
	else
	{
		printf("Error: ROM too big for memory");
	}
	fclose(pFile);
	free(buffer);

	//Emulation loop
	while (true)
	{
		//Emulate one cycle
		sleep_ms(17); //Render delay for fps cap

		//Fetch opcode
		opcode = memory[programcounter] << 8 | memory[programcounter + 1];

		//Decode opcode
		switch (opcode & 0xF000)
		{
		case 0x0000:
			switch (opcode & 0x000F)
			{
			case 0x0000: // 0x00E0: Clears the screen
				for (int i = 0; i < 2048; ++i)
					graphics[i] = 0x0;
				draw = true;
				programcounter += 2;
				break;

			case 0x000E: // 0x00EE: Returns from subroutine
				--stackpointer;			// 16 levels of stack, decrease stack pointer to prevent overwrite
				programcounter = stack[stackpointer];	// Put the stored return address from the stack back into the program counter					
				programcounter += 2;		// Don't forget to increase the program counter!
				break;

			default:
				printf("Unknown opcode [0x0000]: 0x%X\n", opcode);
			}
			break;

		case 0x1000: // 0x1NNN: Jumps to address NNN
			programcounter = opcode & 0x0FFF;
			break;

		case 0x2000: // 0x2NNN: Calls subroutine at NNN.
			stack[stackpointer] = programcounter;			// Store current address in stack
			++stackpointer;					// indexregisterncrement stack pointer
			programcounter = opcode & 0x0FFF;	// Set the program counter to the address at NNN
			break;

		case 0x3000: // 0x3XNN: Skips the next instruction if registersX equals NN
			if (registers[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
				programcounter += 4;
			else
				programcounter += 2;
			break;

		case 0x4000: // 0x4XNN: Skips the next instruction if registersX doesn't equal NN
			if (registers[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
				programcounter += 4;
			else
				programcounter += 2;
			break;

		case 0x5000: // 0x5XY0: Skips the next instruction if registersX equals registersY.
			if (registers[(opcode & 0x0F00) >> 8] == registers[(opcode & 0x00F0) >> 4])
				programcounter += 4;
			else
				programcounter += 2;
			break;

		case 0x6000: // 0x6XNN: Sets registersX to NN.
			registers[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
			programcounter += 2;
			break;

		case 0x7000: // 0x7XNN: Adds NN to registersX.
			registers[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
			programcounter += 2;
			break;

		case 0x8000:
			switch (opcode & 0x000F)
			{
			case 0x0000: // 0x8XY0: Sets registersX to the value of registersY
				registers[(opcode & 0x0F00) >> 8] = registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0001: // 0x8XY1: Sets registersX to "registersX OR registersY"
				registers[(opcode & 0x0F00) >> 8] |= registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0002: // 0x8XY2: Sets registersX to "registersX AND registersY"
				registers[(opcode & 0x0F00) >> 8] &= registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0003: // 0x8XY3: Sets registersX to "registersX XOR registersY"
				registers[(opcode & 0x0F00) >> 8] ^= registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0004: // 0x8XY4: Adds registersY to registersX. registersF is set to 1 when there's a carry, and to 0 when there isn't					
				if (registers[(opcode & 0x00F0) >> 4] > (0xFF - registers[(opcode & 0x0F00) >> 8]))
					registers[0xF] = 1; //carry
				else
					registers[0xF] = 0;
				registers[(opcode & 0x0F00) >> 8] += registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0005: // 0x8XY5: registersY is subtracted from registersX. registersF is set to 0 when there's a borrow, and 1 when there isn't
				if (registers[(opcode & 0x00F0) >> 4] > registers[(opcode & 0x0F00) >> 8])
					registers[0xF] = 0; // there is a borrow
				else
					registers[0xF] = 1;
				registers[(opcode & 0x0F00) >> 8] -= registers[(opcode & 0x00F0) >> 4];
				programcounter += 2;
				break;

			case 0x0006: // 0x8XY6: Shifts registersX right by one. registersF is set to the value of the least significant bit of registersX before the shift
				registers[0xF] = registers[(opcode & 0x0F00) >> 8] & 0x1;
				registers[(opcode & 0x0F00) >> 8] >>= 1;
				programcounter += 2;
				break;

			case 0x0007: // 0x8XY7: Sets registersX to registersY minus registersX. registersF is set to 0 when there's a borrow, and 1 when there isn't
				if (registers[(opcode & 0x0F00) >> 8] > registers[(opcode & 0x00F0) >> 4])	// registersY-registersX
					registers[0xF] = 0; // there is a borrow
				else
					registers[0xF] = 1;
				registers[(opcode & 0x0F00) >> 8] = registers[(opcode & 0x00F0) >> 4] - registers[(opcode & 0x0F00) >> 8];
				programcounter += 2;
				break;

			case 0x000E: // 0x8XYE: Shifts registersX left by one. registersF is set to the value of the most significant bit of registersX before the shift
				registers[0xF] = registers[(opcode & 0x0F00) >> 8] >> 7;
				registers[(opcode & 0x0F00) >> 8] <<= 1;
				programcounter += 2;
				break;

			default:
				printf("Unknown opcode [0x8000]: 0x%X\n", opcode);
			}
			break;

		case 0x9000: // 0x9XY0: Skips the next instruction if registersX doesn't equal registersY
			if (registers[(opcode & 0x0F00) >> 8] != registers[(opcode & 0x00F0) >> 4])
				programcounter += 4;
			else
				programcounter += 2;
			break;

		case 0xA000: // ANNN: Sets indexregister to the address NNN
			indexregister = opcode & 0x0FFF;
			programcounter += 2;
			break;

		case 0xB000: // BNNN: Jumps to the address NNN plus registers0
			programcounter = (opcode & 0x0FFF) + registers[0];
			break;

		case 0xC000: // CXNN: Sets registersX to a random number and NN
			registers[(opcode & 0x0F00) >> 8] = (rand() % 0xFF) & (opcode & 0x00FF);
			programcounter += 2;
			break;

		case 0xD000: // DXYN: Draws a sprite at coordinate (registersX, registersY) that has a width of 8 pixels and a height of N pixels. 
					 // Each row of 8 pixels is read as bit-coded starting from memory location indexregister; 
					 // indexregister value doesn't change after the execution of this instruction. 
					 // registersF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, 
					 // and to 0 if that doesn't happen
		{
			unsigned short x = registers[(opcode & 0x0F00) >> 8];
			unsigned short y = registers[(opcode & 0x00F0) >> 4];
			unsigned short height = opcode & 0x000F;
			unsigned short pixel;

			registers[0xF] = 0;
			for (int yline = 0; yline < height; yline++)
			{
				pixel = memory[indexregister + yline];
				for (int xline = 0; xline < 8; xline++)
				{
					if ((pixel & (0x80 >> xline)) != 0)
					{
						if (graphics[(x + xline + ((y + yline) * 64))] == 1)
						{
							registers[0xF] = 1;
						}
						graphics[x + xline + ((y + yline) * 64)] ^= 1;
					}
				}
			}

			draw = true;
			programcounter += 2;
		}
		break;

		case 0xE000:
			switch (opcode & 0x00FF)
			{
			case 0x009E: // EX9E: Skips the next instruction if the key stored in registersX is pressed
				if (key[registers[(opcode & 0x0F00) >> 8]] != 0)
					programcounter += 4;
				else
					programcounter += 2;
				break;

			case 0x00A1: // EXA1: Skips the next instruction if the key stored in registersX isn't pressed
				if (key[registers[(opcode & 0x0F00) >> 8]] == 0)
					programcounter += 4;
				else
					programcounter += 2;
				break;

			default:
				printf("Unknown opcode [0xE000]: 0x%X\n", opcode);
			}
			break;

		case 0xF000:
			switch (opcode & 0x00FF)
			{
			case 0x0007: // FX07: Sets registersX to the value of the delay timer
				registers[(opcode & 0x0F00) >> 8] = delay_timer;
				programcounter += 2;
				break;

			case 0x000A: // FX0A: A key press is awaited, and then stored in registersX		
			{
				bool keyPress = false;

				for (int i = 0; i < 16; ++i)
				{
					if (key[i] != 0)
					{
						registers[(opcode & 0x0F00) >> 8] = i;
						keyPress = true;
					}
				}

				// indexregisterf we didn't received a keypress, skip this cycle and try again.
				if (!keyPress)
					break;

				programcounter += 2;
			}
			break;

			case 0x0015: // FX15: Sets the delay timer to registersX
				delay_timer = registers[(opcode & 0x0F00) >> 8];
				programcounter += 2;
				break;

			case 0x0018: // FX18: Sets the sound timer to registersX
				sound_timer = registers[(opcode & 0x0F00) >> 8];
				programcounter += 2;
				break;

			case 0x001E: // FX1E: Adds registersX to indexregister
				if (indexregister + registers[(opcode & 0x0F00) >> 8] > 0xFFF)	// registersF is set to 1 when range overflow (indexregister+registersX>0xFFF), and 0 when there isn't.
					registers[0xF] = 1;
				else
					registers[0xF] = 0;
				indexregister += registers[(opcode & 0x0F00) >> 8];
				programcounter += 2;
				break;

			case 0x0029: // FX29: Sets indexregister to the location of the sprite for the character in registersX. Characters 0-F (in hexadecimal) are represented by a 4x5 font
				indexregister = registers[(opcode & 0x0F00) >> 8] * 0x5;
				programcounter += 2;
				break;

			case 0x0033: // FX33: Stores the Binary-coded decimal representation of registersX at the addresses indexregister, indexregister plus 1, and indexregister plus 2
				memory[indexregister] = registers[(opcode & 0x0F00) >> 8] / 100;
				memory[indexregister + 1] = (registers[(opcode & 0x0F00) >> 8] / 10) % 10;
				memory[indexregister + 2] = (registers[(opcode & 0x0F00) >> 8] % 100) % 10;
				programcounter += 2;
				break;

			case 0x0055: // FX55: Stores registers0 to registersX in memory starting at address indexregister					
				for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i)
					memory[indexregister + i] = registers[i];

				// On the original interpreter, when the operation is done, indexregister = indexregister + X + 1.
				indexregister += ((opcode & 0x0F00) >> 8) + 1;
				programcounter += 2;
				break;

			case 0x0065: // FX65: Fills registers0 to registersX with values from memory starting at address indexregister					
				for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i)
					registers[i] = memory[indexregister + i];

				// On the original interpreter, when the operation is done, indexregister = indexregister + X + 1.
				indexregister += ((opcode & 0x0F00) >> 8) + 1;
				programcounter += 2;
				break;

			default:
				printf("Unknown opcode [0xF000]: 0x%X\n", opcode);
			}
			break;

		default:
			printf("Unknown opcode: 0x%X\n", opcode);
		}

		//Update timers
		if (delay_timer > 0)
			--delay_timer;

		if (sound_timer > 0)
		{
			if (sound_timer == 1)
				printf("BEEP!\n");
			--sound_timer;
		}

		// If the draw flag is set, update the screen
		//if (draw)
			//Do graphics rendering code here

		//Store key press state (Press and Release)
		//Do set key code here
	}

	return 0;
}