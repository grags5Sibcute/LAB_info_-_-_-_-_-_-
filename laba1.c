#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define MEMORY_SIZE 256
#define START_PC 6

#define PC 0
#define ACC 1
#define IR 2
#define VECTOR 3
#define HALTED 4
#define ERROR 5

struct Machine
{
    int cpu_state[8];
    int memory[256];
    int loaded_cells;
    int in_handler;
};

/* One memory array holds instructions, addresses and data.
   PC selects the next instruction; ACC stores the arithmetic result. */
static int load_program(struct Machine *vm)
{
    FILE *file = fopen("program.txt", "r");
    if (file == NULL)
    {
        puts("Cannot open program.txt");
        return 0;
    }

    char token[64];
    while (fscanf(file, "%63s", token) == 1)
    {
        int separator = fgetc(file);
        char *end;
        errno = 0;
        long number = strtol(token, &end, 10);

        if (vm->loaded_cells == MEMORY_SIZE ||
            (separator != EOF && !isspace(separator)) ||
            end == token || *end != '\0' || errno == ERANGE ||
            number < INT_MIN || number > INT_MAX)
        {
            printf("Invalid input at memory cell %d\n", vm->loaded_cells);
            fclose(file);
            return 0;
        }

        vm->memory[vm->loaded_cells] = (int)number;
        vm->loaded_cells++;
    }

    int read_failed = ferror(file);
    fclose(file);
    if (read_failed || vm->loaded_cells <= START_PC)
    {
        puts("Cannot read a complete program starting at address 6");
        return 0;
    }
    return 1;
}

static int valid_address(int address)
{
    return address >= 0 && address < MEMORY_SIZE;
}
static void interrupt_machine(struct Machine *vm, int error, const char *reason)
{
    int *cpu = vm->cpu_state;
    cpu[ERROR] = error;
    printf("acc=%d, ERROR %d: %s\n", cpu[ACC], error, reason);

    if (vm->in_handler)
    {
        puts("Repeated interrupt: execution stopped");
        cpu[HALTED] = 1;
    }
    else if (!valid_address(cpu[VECTOR]))
    {
        printf("Invalid interrupt vector: %d\n", cpu[VECTOR]);
        cpu[HALTED] = 1;
    }
    else
    {
        vm->in_handler = 1;
        cpu[PC] = cpu[VECTOR];
        printf("Interrupt: pc <- %d\n", cpu[PC]);
    }
}
static void execute(struct Machine *vm)
{
    int *cpu = vm->cpu_state;
    int *memory = vm->memory;

    while (!cpu[HALTED])
    {
        int pc = cpu[PC];
        if (!valid_address(pc))
        {
            printf("pc=%d: ", pc);
            interrupt_machine(vm, 1, "instruction address out of range");
            continue;
        }
        cpu[IR] = memory[pc];
        int opcode = cpu[IR];
        printf("pc=%d: opcode=%d, ", pc, opcode);

        if (opcode == 0)
        {
            cpu[HALTED] = 1;
            printf("operand=-, acc=%d (HALT)\n", cpu[ACC]);
            continue;
        }
        if (opcode < 1 || opcode > 5)
        {
            puts("operand=-");
            interrupt_machine(vm, 2, "unknown opcode");
            continue;
        }
        if (!valid_address(pc + 1) || pc + 1 >= vm->loaded_cells)
        {
            puts("operand=-");
            interrupt_machine(vm, 3, "missing operand cell");
            continue;
        }
        int operand = memory[pc + 1];
        int next_pc = pc + 2;
        printf("operand=%d, ", operand);

        if (opcode == 5 && cpu[ACC] != 0)
        {
            printf("acc=%d (JZ not taken), next pc=%d\n", cpu[ACC], next_pc);
            cpu[PC] = next_pc;
            continue;
        }
        if (!valid_address(operand))
        {
            interrupt_machine(vm, 4, "operand address out of range");
            continue;
        }

        long long result = cpu[ACC];
        switch (opcode)
        {
            case 1:
                result = memory[operand];
                break;
            case 2:
                result += memory[operand];
                break;
            case 3:
                result -= memory[operand];
                break;
            case 4:
            case 5:
                next_pc = operand;
                break;
        }

        if (result < INT_MIN || result > INT_MAX)
        {
            interrupt_machine(vm, 5, "accumulator overflow");
            continue;
        }
        cpu[ACC] = (int)result;
        cpu[PC] = next_pc;
        printf("acc=%d, next pc=%d\n", cpu[ACC], cpu[PC]);
    }
}
int main(void)
{
    struct Machine vm = {0};
    if (!load_program(&vm))
    {
        return 1;
    }

    vm.cpu_state[PC] = START_PC;
    vm.cpu_state[VECTOR] = vm.memory[0];
    execute(&vm);

    printf("Final: acc=%d, error=%d\n", vm.cpu_state[ACC], vm.cpu_state[ERROR]);
    return vm.cpu_state[ERROR] != 0;
}