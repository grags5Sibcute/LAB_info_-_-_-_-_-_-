#include <stdio.h>

#define MEMORY_SIZE 256
#define START_PC 6
// Диапазон int в используемой 32-битной модели целых чисел.
#define MAX_INT 2147483647LL
#define MIN_INT (-2147483647LL - 1)

// Пробелы, табуляция и переносы строк разделяют числа в файле.
static int is_space(int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' ||
           ch == '\r' || ch == '\v' || ch == '\f';
}

/* Результат: 1 — число прочитано, 0 — конец файла, -1 — ошибка. */
static int read_number(FILE *file, int *number)
{
    int ch = fgetc(file);
    while (is_space(ch))
        ch = fgetc(file);

    if (ch == EOF)
        return 0;

    int negative = ch == '-';
    if (ch == '-' || ch == '+')
        ch = fgetc(file);

    if (ch < '0' || ch > '9')
        return -1;

    // Например, из цифр 1 и 2 получаем 1 * 10 + 2 = 12.
    long long value = 0;
    long long limit = MAX_INT;
    if (negative)
        limit = -MIN_INT;
    while (ch >= '0' && ch <= '9')
    {
        value = value * 10 + (ch - '0');
        if (value > limit)
            return -1;
        ch = fgetc(file);
    }

    // После числа разрешён разделитель или конец файла.
    if (ch != EOF && !is_space(ch))
        return -1;

    if (negative)
        value = -value;
    *number = (int)value;
    return 1;
}

// Команды и данные лежат в одном массиве memory — принцип фон Неймана.
// Структура объединяет эту память и регистры виртуальной машины.
struct Machine {
    int memory[MEMORY_SIZE];
    // 0 — PC, 1 — ACC, 2 — IR, 3 — вектор, 4 — стоп, 5 — ошибка.
    // Ячейки 6 и 7 оставляем в резерве.
    int cpu_state[8];
};

int main(void)
{
    struct Machine machine = {0};
    // Указатели дают короткие имена массивам внутри структуры.
    int *memory = machine.memory;
    int *cpu_state = machine.cpu_state;
    int in_handler = 0;

    FILE *file = fopen("program.txt", "r");
    if (file == NULL) {
        puts("Cannot open program.txt");
        return 1;
    }

    int count = 0;
    int number;
    int read;
    while ((read = read_number(file, &number)) == 1) {
        if (count == MEMORY_SIZE) {
            printf("Invalid input at memory cell %d\n", count);
            fclose(file);
            return 1;
        }
        memory[count] = number;
        count++;
    }

    int read_failed = ferror(file);
    fclose(file);
    if (read == -1 || read_failed || count <= START_PC) {
        puts("Invalid or incomplete program.txt");
        return 1;
    }

    cpu_state[0] = START_PC;
    cpu_state[3] = memory[0];

    while (cpu_state[4] == 0) {
        int pc = cpu_state[0];
        int cmd = -1;
        int addr = 0;
        int error = 0;

        // Проверяем адрес и только затем читаем ячейку.
        if (pc < 0 || pc >= MEMORY_SIZE) {
            error = 1;
        } else {
            cmd = memory[pc];
            cpu_state[2] = cmd;
            if (cmd < 0 || cmd > 5) {
                error = 2;
            } else if (cmd != 0) {
                if (pc + 1 >= count) {
                    error = 3;
                } else {
                    addr = memory[pc + 1];
                    // При ACC != 0 команда 5 пропускает переход.
                    if ((cmd != 5 || cpu_state[1] == 0) &&
                        (addr < 0 || addr >= MEMORY_SIZE))
                        error = 4;
                }
            }
        }

        if (error == 0) {
            // Считаем в long long, чтобы проверить результат до записи в int.
            long long acc = cpu_state[1];
            int next_pc = pc + 2;

            switch (cmd) {
            case 0: // HALT: остановиться.
                cpu_state[4] = 1;
                break;
            case 1: // LOAD: взять значение по адресу.
                acc = memory[addr];
                break;
            case 2: // ADD: прибавить значение.
                acc += memory[addr];
                break;
            case 3: // SUB: вычесть значение.
                acc -= memory[addr];
                break;
            case 4: // JUMP: перейти всегда.
                next_pc = addr;
                break;
            case 5: // JZ: перейти только при нулевом ACC.
                if (acc == 0)
                    next_pc = addr;
                break;
            }

            if (acc < MIN_INT || acc > MAX_INT) {
                error = 5;
            } else {
                cpu_state[1] = (int)acc;
                if (cmd == 0) {
                    printf("pc=%d: HALT, acc=%d\n", pc, cpu_state[1]);
                } else {
                    cpu_state[0] = next_pc;
                    printf("pc=%d: cmd=%d, addr=%d, acc=%d, next pc=%d\n",
                           pc, cmd, addr, cpu_state[1], next_pc);
                }
            }
        }

        // Обрабатываем любую ошибку здесь. Второй сбой завершает работу.
        if (error != 0) {
            cpu_state[5] = error;
            printf("pc=%d: error=%d\n", pc, error);
            if (in_handler) {
                puts("Repeated interrupt: execution stopped");
                cpu_state[4] = 1;
            } else if (cpu_state[3] < 0 || cpu_state[3] >= MEMORY_SIZE) {
                printf("Invalid interrupt vector: %d\n", cpu_state[3]);
                cpu_state[4] = 1;
            } else {
                in_handler = 1;
                cpu_state[0] = cpu_state[3];
                printf("Interrupt: pc <- %d\n", cpu_state[0]);
            }
        }
    }

    printf("Final: acc=%d, error=%d\n", cpu_state[1], cpu_state[5]);
    return cpu_state[5] != 0;
}
