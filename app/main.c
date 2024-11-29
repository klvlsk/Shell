#include <stdio.h>
//функции ввода/вывода
#include <stdlib.h>
//работа с памятью
#include <string.h>
//обработка строк
#include <unistd.h>
//системные вызовы
#include <sys/types.h>
//типы данных для системных вызовов
#include <signal.h>
//работа с сигналами
#include <stdint.h>
//стандартные типы данных фиксированной ширины
#include <fcntl.h>
//работа с файлами
#include <sys/wait.h>
//управление процессами

// Задание 8: Обработчик сигнала SIGHUP
void sighup_handler(int signal_number) {
    // Обработчик для сигнала SIGHUP: выводит сообщение о перезагрузке конфигурации
    const char *message = "Configuration reloaded\n";
    if (write(STDOUT_FILENO, message, strlen(message)) == -1) {
        perror("Ошибка записи сообщения");
    }
}

int main() {

    // Выводим PID текущего процесса
    printf("Current PID: %d\n", getpid());

    // Задание 3: Сохранение истории команд в файл
    // Открываем файл для записи истории команд в режиме добавления
    FILE *command_history = fopen("history.txt", "a");
    if (!command_history) {
        perror("Не удалось открыть файл истории");
        return 1;
    }

    // Задание 8: Установка обработчика сигнала SIGHUP
    // Назначаем обработчик для SIGHUP и проверяем успешность установки
    if (signal(SIGHUP, sighup_handler) == SIG_ERR) {
        perror("Не удалось установить обработчик сигнала");
    }

    while (1) {
        // Задание 1: Выход по Ctrl+D
        // Печатаем приглашение командной строки
        printf("$ ");
        fflush(stdout);

        // Буфер для ввода пользователя
        char user_input[100];

        // Считываем ввод пользователя; если NULL, завершаем программу
        if (!fgets(user_input, sizeof(user_input), stdin)) {
            fclose(command_history);
            return 0;
        }

        // Удаляем символ новой строки в конце строки
        size_t input_length = strlen(user_input);
        if (input_length > 0 && user_input[input_length - 1] == '\n') {
            user_input[input_length - 1] = '\0';
        }

        // Записываем ввод пользователя в файл истории
        if (fputs(user_input, command_history) == EOF || fputc('\n', command_history) == EOF) {
            perror("Ошибка записи в файл истории");
        }

        // Переменные для хранения первых символов ввода для проверки команд
        char command_prefix[100] = {0};
        strncpy(command_prefix, user_input, 4);
        char extended_prefix[10] = {0};
        strncpy(extended_prefix, user_input, 8);

        // Задание 4: Реализация команды echo
        // Если команда начинается с "echo", выводим оставшуюся часть строки
        if (strcmp(command_prefix, "echo") == 0) {
            printf("%s\n", user_input + 5);
        }

        // Задание 6: Вывод значения переменной окружения
        // Если команда начинается с "\e $", пытаемся получить значение переменной окружения
        else if (strcmp(command_prefix, "\\e $") == 0) {
            const char *env_var_name = user_input + 4; // Имя переменной окружения
            const char *env_var_value = getenv(env_var_name); // Значение переменной окружения
            printf("%s = %s\n", env_var_name, env_var_value ? env_var_value : "NULL");
        }


        // Задание 7: Выполнение указанного бинарника
        // Если команда начинается с "/bin", запускаем указанный бинарник
        else if (strcmp(command_prefix, "/bin") == 0) {
            const char* binary_name = user_input + 4; // Имя бинарника
            pid_t child_pid = fork(); // Создаем дочерний процесс
            if (child_pid < 0) {
                perror("Ошибка fork");
            }
            else if (child_pid == 0) {
                // В дочернем процессе
                execl(user_input, binary_name, NULL); // Исполняем бинарник
                perror("Ошибка execl"); // Если execl завершился с ошибкой
                exit(1); // Завершаем дочерний процесс
            }
            else {
                // В родительском процессе ждем завершения дочернего процесса
                int status;
                waitpid(child_pid, &status, 0); // Ожидаем завершения дочернего процесса
            }
        }

        // Задание 9: Проверка, загрузочный ли диск
        // Если команда начинается с "\l /dev/", проверяем диск на загрузочность
        else if (strcmp(extended_prefix, "\\l /dev/") == 0) {
            const char *disk_path = user_input + 3; // Путь к диску
            FILE *disk_file = fopen(disk_path, "rb"); // Открываем файл для чтения
            if (!disk_file) {
                printf("No such file or directory\n");
                continue;
            }
            if (fseek(disk_file, 510, SEEK_SET) != 0) {
                perror("Ошибка при смещении");
                fclose(disk_file);
                continue;
            }
            uint8_t boot_signature[2]; // Буфер для загрузочной подписи
            if (fread(boot_signature, 1, 2, disk_file) != 2) {
                perror("Ошибка при чтении");
                fclose(disk_file);
                continue;
            }
            fclose(disk_file);

            // Проверяем, является ли диск загрузочным
            printf("Диск %sзагрузочный\n",
                   (boot_signature[0] == 0x55 && boot_signature[1] == 0xAA) ? "" : "не ");
        }

        // Задание 10: Получение дампа памяти процесса
        // Если команда начинается с "\mem", создаем дамп памяти указанного процесса
        else if (strcmp(command_prefix, "\\mem") == 0) {
            pid_t target_pid = atoi(user_input + 4); // PID целевого процесса
            const char *dump_file = "damp_proc"; // Файл для дампа памяти
            char maps_file_path[256], mem_file_path[256], map_line[256]; // Пути к файлам
            snprintf(maps_file_path, sizeof(maps_file_path), "/proc/%d/maps", target_pid); // maps файл
            snprintf(mem_file_path, sizeof(mem_file_path), "/proc/%d/mem", target_pid); // mem файл

            FILE *maps_file = fopen(maps_file_path, "r"); // Открываем maps файл
            if (!maps_file) {
                perror("Cannot open maps");
                continue;
            }

            int mem_fd = open(mem_file_path, O_RDONLY); // Открываем mem файл
            if (mem_fd == -1) {
                perror("Cannot open mem");
                fclose(maps_file);
                continue;
            }

            int dump_fd = open(dump_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Открываем файл для дампа
            if (dump_fd == -1) {
                perror("Cannot open output file");
                fclose(maps_file);
                close(mem_fd);
                continue;
            }


            // Читаем регионы памяти из maps и дампим их
            while (fgets(map_line, sizeof(map_line), maps_file)) {
                unsigned long region_start, region_end;
                if (sscanf(map_line, "%lx-%lx", &region_start, &region_end) == 2) {
                    char mem_buffer[4096]; // Буфер для чтения памяти
                    for (unsigned long address = region_start; address < region_end; address += sizeof(mem_buffer)) {
                        ssize_t bytes_read = pread(mem_fd, mem_buffer, sizeof(mem_buffer), address); // Чтение памяти
                        if (bytes_read > 0) {
                            write(dump_fd, mem_buffer, bytes_read); // Запись в дамп
                        }
                    }
                }
            }

            fclose(maps_file);
            close(mem_fd);
            close(dump_fd);

            printf("Dump saved to %s\n", dump_file); // Уведомляем о завершении дампа
        }

        // Задание 2: Выход по exit и по \q
        // Если команда "exit" или "\q", завершаем выполнение программы
        else if (strcmp(user_input, "exit") == 0 || strcmp(user_input, "\\q") == 0) {
            fclose(command_history); // Закрываем файл истории
            return 0; // Завершаем программу
        }

        // Задание 5: Проверка введённой команды
        // Если команда не распознана, выводим сообщение об ошибке
        else {
            printf("%s: command not found\n", user_input);
        }
    }
}
