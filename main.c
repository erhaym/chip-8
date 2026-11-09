#include <stdint.h>
#include <string.h>
#include <stdio.h>

enum GeneralPurposeRegisters {
    V0, V1, V2, V3, V4, V5, V6, V7,
    V8, V9, VA, VB, VC, VD, VE, VF
};

enum SpecialPurposeRegisters {
    I, PC, SP
};

enum Timers {
    // delay and Sound timers
    DT, ST
};

typedef struct {
    uint8_t memory[4096];
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

    cpu->sp_registers[PC] = 0x200;
}

int main(void) {
    // initialize all members of the struct to 0
    CHIP8_CPU cpu;

    init(&cpu);

    return 0;
}
