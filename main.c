#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MEMORY_SIZE 4096
#define ROM_START 0x200

enum GeneralPurposeRegisters {
    V0, V1, V2, V3, V4, V5, V6, V7,
    V8, V9, VA, VB, VC, VD, VE, VF
};

enum SpecialPurposeRegisters {
    // I: 16-bit index register used to store memory addresses
    // used to point to locations in memory
    I, PC, SP
};

enum Timers {
    // delay and Sound timers
    DT, ST
};

typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t gp_registers[16];
    uint16_t sp_registers[3];
    uint8_t timers[2];
    // 64 x 32 display
    uint8_t framebuffer[32][64];
    /*
    the CHIP-8 stack only stores return addresses
    and addresses are 12 bits wide as per the spec 
    */
    uint16_t stack[16];
    uint8_t keypad[16];
} CHIP8_CPU;

void init(CHIP8_CPU *cpu) {
    // initialize all the bytes of the structure to zero
    memset(cpu, 0, sizeof(*cpu));

    cpu->sp_registers[PC] = ROM_START;
}

int load_rom(CHIP8_CPU *cpu, char *path) {
    FILE *fptr;

    fptr = fopen(path, "rb");

    if (fptr == NULL) {
        fprintf(stderr, "ERROR: Could not open file\n");
        return 1;
    }

    // (stream, offset, origin)
    int seek_code = fseek(fptr, 0, SEEK_END);

    if (seek_code != 0) {
        fprintf(stderr, "ERROR: Could not move position\n");
        fclose(fptr);
        return 1;
    }

    long pos_eof = ftell(fptr);

    if (pos_eof == -1L) {
        fprintf(stderr, "ERROR: Could not tell position\n");
        fclose(fptr);
        return 1;
    }

    // moving back to the beginning of the file
    seek_code = fseek(fptr, 0, SEEK_SET);

    if (seek_code != 0) {
        fprintf(stderr, "ERROR: Could not move position\n");
        fclose(fptr);
        return 1;
    }

    if (pos_eof > MEMORY_SIZE - ROM_START) {
        fprintf(stderr, "ERROR: File is too large\n");
        fclose(fptr);
        return 1;
    }

    // long -> size_t casting
    size_t file_size = (size_t)pos_eof;

    // printf("ROM size: %zu\n", file_size);

    size_t bytes_read = fread((cpu->memory + ROM_START), sizeof(char), file_size, fptr);

    if (bytes_read != file_size) {
        fprintf(stderr, "ERROR: File could not be read entirely\n");
        fclose(fptr);
        return 1;
    }

    // printf("ROM loaded at %03X\n", ROM_START);

    // printf("Memory contents:\n");

    /*

    for (size_t i = 0; i < file_size; i++) {
        printf("0x%03X: %02X\n", (0x200 + (unsigned int)i), *(cpu->memory + (0x200 + (int) i)));
    }

    */

    fclose(fptr);
    
    return 0;
}

int fetch(CHIP8_CPU *cpu, uint16_t *instr) {
    if (cpu->sp_registers[PC] >= 0xFFF) {
        fprintf(stderr, "ERROR: Out of memory\n");
        return 1;
    }

    uint8_t instr_h = cpu->memory[cpu->sp_registers[PC]];
    uint8_t instr_l = cpu->memory[cpu->sp_registers[PC] + 1];

    *instr = (instr_h << 8) | instr_l;

    cpu->sp_registers[PC] = cpu->sp_registers[PC] + 2;

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: Please specify one argument (ROM file path)\n");
        return 1;
    }

    char *rom_path = argv[1];

    // initialize all members of the struct to 0
    CHIP8_CPU cpu;

    init(&cpu);

    int load_code = load_rom(&cpu, rom_path);

    if (load_code == 1) {
        fprintf(stderr, "ERROR: ROM could not be loaded\n");
        return 1;
    }

    uint16_t instr_1;
    int c1 = fetch(&cpu, &instr_1);

    if (c1 == 1) {
        return 1;
    }

    uint16_t instr_2;
    int c2 = fetch(&cpu, &instr_2);

    if (c2 == 1) {
        return 1;
    }

    printf("Instruction 1: %04X\nInstruction 2: %04X\n", instr_1, instr_2);

    return 0;
}
