#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// addresses are 12 bits long
#define MEMORY_SIZE 4096
#define ROM_START 0x200

enum GeneralPurposeRegisters {
    // VF: also used as a flag register
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
    // memory is byte-adressable
    uint8_t memory[MEMORY_SIZE];
    // general-purpose registers
    uint8_t gp_registers[16];
    // special-purpose registers
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

    size_t bytes_read = fread((cpu->memory + ROM_START), sizeof(char), file_size, fptr);

    if (bytes_read != file_size) {
        fprintf(stderr, "ERROR: File could not be read entirely\n");
        fclose(fptr);
        return 1;
    }

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

int execute(CHIP8_CPU *cpu, uint8_t op, uint16_t nnn, uint8_t nn, uint8_t n, uint8_t x, uint8_t y) {

    switch (op) {
        // REGISTER ARITHMETIC AND BIT OPS
        // VX := NN
        case 0x6:
            cpu->gp_registers[x] = nn;
            break;
        // VX += NN
        case 0x7:
            cpu->gp_registers[x] += nn;
            break;
        // I = NNN
        case 0xA:
            cpu->sp_registers[I] = nnn;
            break;
        case 0x8:
            switch(n) {
                // VX := VY
                case 0x0:
                    cpu->gp_registers[x] = cpu->gp_registers[y];
                    break;

                // VX := VX ∨ VY
                case 0x1:
                    cpu->gp_registers[x] = cpu->gp_registers[x] | cpu->gp_registers[y];
                    break;

                // VX := VX ∧ VY
                case 0x2:
                    cpu->gp_registers[x] = cpu->gp_registers[x] & cpu->gp_registers[y];
                    break;

                // VX := VX ⊕ VY
                case 0x3:
                    cpu->gp_registers[x] = cpu->gp_registers[x] ^ cpu->gp_registers[y];
                    break;

                // VX := VX + VY
                case 0x4:
                    if ((cpu->gp_registers[x] + cpu->gp_registers[y]) > 255) {
                        cpu->gp_registers[x] = cpu->gp_registers[x] + cpu->gp_registers[y];
                        // carry occured
                        cpu->gp_registers[VF] = 1;
                        break;
                    }

                    cpu->gp_registers[x] = cpu->gp_registers[x] + cpu->gp_registers[y];
                    // carry didn't occur
                    cpu->gp_registers[VF] = 0;
                    break;

                // VX := VX - VY
                case 0x5:
                    if ((cpu->gp_registers[x] - cpu->gp_registers[y]) < 0) {
                        cpu->gp_registers[x] = cpu->gp_registers[x] - cpu->gp_registers[y];
                        // borrow occured
                        cpu->gp_registers[VF] = 0;
                        break;
                    }

                    cpu->gp_registers[x] = cpu->gp_registers[x] - cpu->gp_registers[y];
                    // borrow did not occur
                    cpu->gp_registers[VF] = 1;
                    break;

                // VX := VX >> 1;
                case 0x6: {
                    // since VF is a GPR it's possible that x = 0xF

                    uint8_t vx_cpy = cpu->gp_registers[x];

                    cpu->gp_registers[x] >>= 1;

                    cpu->gp_registers[VF] = vx_cpy & 0x01;
                    break;
                }

                // VX := VY - VX
                case 0x7:
                    if ((cpu->gp_registers[y] - cpu->gp_registers[x]) < 0) {
                        cpu->gp_registers[x] = cpu->gp_registers[y] - cpu->gp_registers[x];
                        // borrow occured
                        cpu->gp_registers[VF] = 0;
                        break;
                    }

                    cpu->gp_registers[x] = cpu->gp_registers[y] - cpu->gp_registers[x];
                    // borrow did not occur
                    cpu->gp_registers[VF] = 1;
                    break;

                // VX := VX >> 1;
                case 0xE: {
                    // since VF is a GPR it's possible that x = 0xF

                    uint8_t vx_cpy = cpu->gp_registers[x];

                    cpu->gp_registers[x] <<= 1;

                    cpu->gp_registers[VF] = (vx_cpy & 0x80) >> 7;
                    break;
                }

                default:
                    fprintf(stderr, "ERROR: Unrecognized instruction '%X%04X'\n", op, nnn);
                    return 1;
            }

            break;
        // ------------------------------------------------------------
        // JUMPS
        case 0x1:
            cpu->sp_registers[PC] = nnn;
            break;

        case 0x3:
            if (cpu->gp_registers[x] == nn) {
                cpu->sp_registers[PC] += 2;
            }
            break;

        case 0x4:
            if (cpu->gp_registers[x] != nn) {
                cpu->sp_registers[PC] += 2;
            }
            break;

        case 0x5:
            if (n != 0) {
                fprintf(stderr, "ERROR: Invalid instruction\n");
                return 1;
            }

            if (cpu->gp_registers[x] == cpu->gp_registers[y]) {
                cpu->sp_registers[PC] += 2;
            }
            break;

        case 0x9:
            if (n != 0) {
                fprintf(stderr, "ERROR: Invalid instruction\n");
                return 1;
            }

            if (cpu->gp_registers[x] != cpu->gp_registers[y]) {
                cpu->sp_registers[PC] += 2;
            }
            break;
        // ------------------------------------------------------------
        // SUBROUTINES
        case 0x2:
            if (cpu->sp_registers[SP] >= 16) {
                fprintf(stderr, "ERROR: Stack overflow\n");
                return 1;
            }

            cpu->stack[cpu->sp_registers[SP]] = cpu->sp_registers[PC];
            cpu->sp_registers[SP]++;
            cpu->sp_registers[PC] = nnn;
            break;

        case 0x0:
            switch(nnn) {
                case 0x0EE:
                    if (cpu->sp_registers[SP] == 0) {
                        fprintf(stderr, "ERROR: Stack underflow\n");
                        return 1;
                    }

                    cpu->sp_registers[SP]--;
                    cpu->sp_registers[PC] = cpu->stack[cpu->sp_registers[SP]];
                    break;
                // case ...:
                default:
                    fprintf(stderr, "ERROR: Unrecognized instruction '%X%04X'\n", op, nnn);
                    return 1;
            }
            break;
        // -----------------------------------------------------------

        default:
            fprintf(stderr, "ERROR: Unrecognized operation '%X'\n", op);
            return 1;
    }

    return 0;
}

int decode(CHIP8_CPU *cpu, uint16_t instr) {
    uint8_t op = instr >> 12;
    uint16_t nnn = instr & 0x0FFF;
    uint8_t nn = nnn & 0x00FF;
    uint8_t n = nn & 0x0F;
    uint8_t x = (instr & 0x0F00) >> 8;
    uint8_t y = (instr & 0x00F0) >> 4;

    /* 
        execute() essentially acts as a helper function
        for decode() in this current version, a separation
        between the functions' responsibilities may be required
    */

    int exec_code = execute(cpu, op, nnn, nn, n, x, y);

    if (exec_code == 1) {
        return 1;
    }

    return 0;
}

int step(CHIP8_CPU *cpu) {
    uint16_t instr;

    int f_status;

    f_status = fetch(cpu, &instr);

    if (f_status == 1) {
        return 1;
    } 

    // decode, execute
    int x_status = decode(cpu, instr);

    if (x_status == 1) {
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: Please specify one argument (ROM file path)\n");
        return 1;
    }

    char *rom_path = argv[1];
    
    CHIP8_CPU cpu;

    init(&cpu);

    int load_code = load_rom(&cpu, rom_path);

    if (load_code == 1) {
        fprintf(stderr, "ERROR: ROM could not be loaded\n");
        return 1;
    }

    return 0;
}
