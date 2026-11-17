#include <stdio.h>
#define CPU_STATE_SIZE 8
#define MEMORY_SIZE 256
#define START_PC 6  /* 0 - вектор, 1-5 - данные, 6 - первая команда */

int init_memory(int *memory);
void print_info(int pc, int cmd, int addr, int acc, int next_pc);
int check_repeated_interrupt(int *cpu_state);

int main(void)
{
    /* Команды и данные лежат в одной памяти memory.
       Это принцип архитектуры фон Неймана.
       PC выбирает команду, IR хранит ее код, ACC - результат.
       Обычно PC растет на 2, а переходы задают другой адрес. */
    int cpu_state[CPU_STATE_SIZE] = {0};
    int memory[MEMORY_SIZE] = {0};
    int kolichestvo = init_memory(memory);

    if (kolichestvo <= START_PC)
    {
        printf("Input error: not enough numbers\n");
        return 1;
    }

    cpu_state[0] = START_PC;
    cpu_state[3] = memory[0];

    while (cpu_state[4] == 0)
    {
        int pc = cpu_state[0];
        int cmd = 0;
        int addr = 0;
        int oshibka = 0;

        /* Ошибки: 1 - неверный PC, 2 - неизвестная команда,
           3 - нет операнда, 4 - неверный адрес операнда. */
        if (pc < 0 || pc >= MEMORY_SIZE)
            oshibka = 1;
        else
        {
            cmd = memory[pc];
            cpu_state[2] = cmd;

            if (cmd < 0 || cmd > 5)
                oshibka = 2;
            else if (cmd != 0)
            {
                if (pc + 1 >= kolichestvo)
                    oshibka = 3;
                else
                {
                    addr = memory[pc + 1];
                    if (addr < 0 || addr >= MEMORY_SIZE)
                        oshibka = 4;
                }
            }
        }

        if (oshibka == 0)
        {
            switch (cmd)
            {
                case 0:
                    cpu_state[4] = 1;
                    break;
                case 1:
                    cpu_state[1] = memory[addr];
                    cpu_state[0] += 2;
                    break;
                case 2:
                    cpu_state[1] += memory[addr];
                    cpu_state[0] += 2;
                    break;
                case 3:
                    cpu_state[1] -= memory[addr];
                    cpu_state[0] += 2;
                    break;
                case 4:  /* Безусловный переход */
                    cpu_state[0] = addr;
                    break;
                case 5:  /* Переход, если ACC равен нулю */
                    if (cpu_state[1] == 0)
                        cpu_state[0] = addr;
                    else
                        cpu_state[0] += 2;
                    break;
            }
            print_info(pc, cmd, addr, cpu_state[1], cpu_state[0]);
        }
        else
        {
            cpu_state[5] = oshibka;
            printf("pc=%d: error=%d\n", pc, oshibka);

            if (check_repeated_interrupt(cpu_state))
                break;

            if (cpu_state[3] < 0 || cpu_state[3] >= MEMORY_SIZE)
            {
                printf("Invalid interrupt vector\n");
                cpu_state[4] = 1;
                break;
            }

            /* Запоминаем, что переход к обработчику уже был. */
            cpu_state[6] = 1;
            cpu_state[0] = cpu_state[3];
            printf("Interrupt: pc <- %d\n", cpu_state[0]);
        }
    }

    printf("Final: acc=%d, error=%d\n", cpu_state[1], cpu_state[5]);
    return cpu_state[5] != 0;
}

int init_memory(int *memory)
{
    int i = 0;
    /* При запуске с < program.txt scanf читает числа из файла. */
    while (i < MEMORY_SIZE && scanf("%d", &memory[i]) == 1)
        i++;
    return i;
}

void print_info(int pc, int cmd, int addr, int acc, int next_pc)
{
    if (cmd == 0)
        printf("pc=%d: HALT, acc=%d\n", pc, acc);
    else
        printf("pc=%d: cmd=%d, addr=%d, acc=%d, next pc=%d\n", pc, cmd, addr, acc, next_pc);
}

int check_repeated_interrupt(int *cpu_state)
{
    if (cpu_state[6] == 1)
    {
        printf("Repeated interrupt: execution stopped\n");
        cpu_state[4] = 1;
        return 1;
    }
    return 0;
}
