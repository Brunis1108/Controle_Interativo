// --- Bibliotecas ---
#include <stdio.h>         // Biblioteca padrão de entrada/saída
#include "pico/stdlib.h"   // Biblioteca da Raspberry Pi Pico
#include "hardware/gpio.h" // Controle de GPIOs
#include "hardware/i2c.h"  // Controle de comunicação I2C
#include "FreeRTOS.h"      // Núcleo do FreeRTOS
#include "task.h"          // Manipulação de tarefas do FreeRTOS
#include "semphr.h"        // Manipulação de semáforos do FreeRTOS
#include "lib/ssd1306.h"   // Biblioteca para o display OLED SSD1306

// --- Definições de pinos e constantes ---
#define MAX_USUARIOS 15     // Número máximo de usuários permitidos
#define Button_A 5         // Botão de entrada no GPIO 5
#define Button_B 6         // Botão de saída no GPIO 6
#define button_Joystick 22 // Botão do joystick no GPIO 22

#define led_red 13   // LED vermelho no GPIO 13
#define led_blue 12  // LED azul no GPIO 12
#define led_green 11 // LED verde no GPIO 11

#define BUZZER_PIN 10 // Pino do buzzer no GPIO 10
#define REST 0        // Frequência zero representa silêncio (descanso)

// --- Configuração I2C para o display OLED ---
#define I2C_PORT i2c1  // Porta I2C a ser usada (i2c1)
#define PIN_I2C_SDA 14 // Pino SDA no GPIO 14
#define PIN_I2C_SCL 15 // Pino SCL no GPIO 15
#define endereco 0x3C  // Endereço I2C padrão do display SSD1306

// --- Objetos e Semáforos ---
ssd1306_t ssd; // Objeto de controle do display SSD1306

SemaphoreHandle_t semEntrada;         // Semáforo para controlar entradas
SemaphoreHandle_t semSaida;           // Semáforo para controlar saídas
SemaphoreHandle_t semReset;           // Semáforo para resetar sistema
SemaphoreHandle_t xMutexDisplay;      // Mutex para controle exclusivo do display
SemaphoreHandle_t xSemaphoreContagem; // Semáforo de contagem para número de usuários

// --- Variáveis globais ---
int users = 0; // Quantidade atual de usuários no ambiente

// --- Parâmetros do PWM (caso seja usado no buzzer) ---
const uint16_t period = 4096;   // Período do PWM (não usado diretamente aqui)
const float divider_pwm = 16.0; // Divisor de frequência do PWM (não usado diretamente)

// --- Protótipos das funções ---
void debounce(uint gpio, uint32_t events);
void display_init();
void led_init(uint pin);
void button_init(uint gpio);
void update_display();
void update_leds();
void buzzer_init();
void buzzer_play_note(int freq, int duration_ms);
void vTaskEntrada(void *params);
void vTaskSaida(void *params);
void vTaskReset(void *params);


// --- Função principal ---
int main()
{
    stdio_init_all(); // Inicializa comunicação serial
    sleep_ms(1000);   // Aguarda 1 segundo

    printf("Iniciando...\n");
    display_init(); // Inicializa display OLED

    led_init(led_red); // Inicializa LEDs
    led_init(led_green);
    led_init(led_blue);

    button_init(Button_A); // Inicializa botões
    button_init(Button_B);
    button_init(button_Joystick);

    buzzer_init(); // Inicializa buzzer

    update_display(); // Exibe status inicial
    update_leds();    // Atualiza LEDs

    // Configura interrupções dos botões
    gpio_set_irq_enabled_with_callback(Button_A, GPIO_IRQ_EDGE_FALL, true, debounce);
    gpio_set_irq_enabled(Button_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(button_Joystick, GPIO_IRQ_EDGE_FALL, true);

    printf("%d\n", users); // Exibe número inicial de usuários

    // Criação de semáforos
    semEntrada = xSemaphoreCreateBinary();
    semSaida = xSemaphoreCreateBinary();
    semReset = xSemaphoreCreateBinary();
    xMutexDisplay = xSemaphoreCreateMutex();
    xSemaphoreContagem = xSemaphoreCreateCounting(MAX_USUARIOS, MAX_USUARIOS);

    // Criação de tarefas
    xTaskCreate(vTaskEntrada, "Entrada", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "Saida", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "Reset", 1024, NULL, 1, NULL);

    printf("Iniciando scheduler...\n");
    vTaskStartScheduler(); // Inicia o escalonador do FreeRTOS
    panic_unsupported();   // Fallback de erro (não deve chegar aqui)

    return 0;
}

// --- Tarefa: Entrada de usuários ---
void vTaskEntrada(void *params)
{
    while (true)
    {
        printf("Entrou na tarefa de entrada\n");

        if (xSemaphoreTake(semEntrada, portMAX_DELAY) == pdTRUE) // Aguarda botão de entrada
        {
            if (xSemaphoreTake(xSemaphoreContagem, 0) == pdTRUE) // Verifica se ainda há espaço
            {
                buzzer_play_note(440, 100); // Toca nota A4 (indicando entrada permitida)
                users++;                    // Incrementa número de usuários
                update_leds();              // Atualiza LEDs conforme estado

                if (xMutexDisplay) // Atualiza display com proteção de mutex
                {
                    xSemaphoreTake(xMutexDisplay, portMAX_DELAY);
                    update_display();
                    xSemaphoreGive(xMutexDisplay);
                }
            }
            else
            {
                // Capacidade cheia - sinaliza e exibe mensagem
                buzzer_play_note(400, 300); // Alerta sonoro de lotação
                gpio_put(led_red, true);    // Liga LED vermelho
                gpio_put(led_green, true);  // Liga LED verde

                if (xMutexDisplay)
                {
                    xSemaphoreTake(xMutexDisplay, portMAX_DELAY);
                    ssd1306_fill(&ssd, false);
                    ssd1306_draw_string(&ssd, "Laboratorio", 0, 30);
                    ssd1306_draw_string(&ssd, "cheio", 70, 40);
                    ssd1306_send_data(&ssd);
                    sleep_ms(2000);   // Aguarda 2 segundos
                    update_display(); // Restaura display padrão
                    xSemaphoreGive(xMutexDisplay);
                }

                gpio_put(led_red, false); // Desliga LEDs
                gpio_put(led_green, false);
                update_leds();
            }
        }
    }
}

// --- Tarefa: Saída de usuários ---
void vTaskSaida(void *params)
{
    while (true)
    {
        printf("Entrou na tarefa de saida\n");

        if (xSemaphoreTake(semSaida, portMAX_DELAY) == pdTRUE) // Aguarda botão de saída
        {
            if (users > 0) // Se houver usuários
            {
                buzzer_play_note(349, 100);         // Toca nota F4
                users--;                            // Decrementa número de usuários
                xSemaphoreGive(xSemaphoreContagem); // Libera uma vaga

                update_leds();

                if (xMutexDisplay)
                {
                    xSemaphoreTake(xMutexDisplay, portMAX_DELAY);
                    update_display();
                    xSemaphoreGive(xMutexDisplay);
                }
            }
            else
            {
                // Nenhum usuário presente
                gpio_put(led_blue, false);
                buzzer_play_note(349, 300); // Alerta de vazio

                gpio_put(led_red, true);
                gpio_put(led_green, true);

                if (xMutexDisplay)
                {
                    xSemaphoreTake(xMutexDisplay, portMAX_DELAY);
                    ssd1306_fill(&ssd, false);
                    ssd1306_draw_string(&ssd, "Laboratorio", 0, 30);
                    ssd1306_draw_string(&ssd, "vazio", 70, 40);
                    ssd1306_send_data(&ssd);
                    sleep_ms(1000);
                    update_display();
                    xSemaphoreGive(xMutexDisplay);
                }

                gpio_put(led_red, false);
                gpio_put(led_green, false);
                update_leds();
            }
        }
    }
}

// --- Tarefa: Reset do sistema ---
void vTaskReset(void *params)
{
    while (true)
    {
        printf("Entrou na tarefa de reset\n");

        if (xSemaphoreTake(semReset, portMAX_DELAY) == pdTRUE) // Aguarda botão de reset
        {
            buzzer_play_note(523, 100); // Toca nota C5
            for (int i = 0; i < MAX_USUARIOS; i++)
                xSemaphoreGive(xSemaphoreContagem); // Libera todas as vagas
            users = 0;                              // Reinicia contador

            update_leds();

            if (xMutexDisplay)
            {
                xSemaphoreTake(xMutexDisplay, portMAX_DELAY);
                ssd1306_fill(&ssd, false);
                ssd1306_draw_string(&ssd, "Resetando", 20, 30);
                ssd1306_send_data(&ssd);
                sleep_ms(1000);
                update_display();
                xSemaphoreGive(xMutexDisplay);
            }
        }
    }
}

// --- Interrupção com debounce para botões ---
void debounce(uint gpio, uint32_t events)
{
    static uint32_t last_time = 0;
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time > 200000) // Verifica intervalo mínimo de 200ms
    {
        last_time = current_time;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Identifica qual botão foi pressionado
        if (gpio == Button_A)
            xSemaphoreGiveFromISR(semEntrada, &xHigherPriorityTaskWoken);
        else if (gpio == Button_B)
            xSemaphoreGiveFromISR(semSaida, &xHigherPriorityTaskWoken);
        else if (gpio == button_Joystick)
            xSemaphoreGiveFromISR(semReset, &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// --- Atualiza informações no display OLED ---
void update_display()
{
    ssd1306_fill(&ssd, false);

    char ocupacao[25];
    char capacidade[25];
    char status[20];

    sprintf(ocupacao, "Qtd Atual: %02d", users);
    sprintf(capacidade, "Qtd Max  : %02d", MAX_USUARIOS);

    if (users == MAX_USUARIOS)
        sprintf(status, "<LOTADO>");
    else if (users >= MAX_USUARIOS - 2)
        sprintf(status, "<QUASE CHEIO>");
    else
        sprintf(status, "<DISPONIVEL>");

    ssd1306_draw_string(&ssd, "LABORATORIO", 20, 0);
    ssd1306_line(&ssd, 3, 11, 123, 11, true);
    ssd1306_draw_string(&ssd, ocupacao, 0, 20);
    ssd1306_draw_string(&ssd, capacidade, 0, 35);
    ssd1306_line(&ssd, 3, 45, 123, 45, true);
    ssd1306_draw_string(&ssd, status, 15, 50);

    ssd1306_send_data(&ssd);
}

// --- Inicializa o display OLED ---
void display_init()
{
    printf("Iniciando display...\n");
    i2c_init(I2C_PORT, 400 * 1000); // Inicializa I2C a 400kHz
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
}

// --- Inicializa LED ---
void led_init(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, false);
}

// --- Inicializa botão ---
void button_init(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

// --- Atualiza estado dos LEDs com base na ocupação ---
void update_leds()
{
    gpio_put(led_red, false);
    gpio_put(led_green, false);
    gpio_put(led_blue, false);

    if (users == MAX_USUARIOS)
        gpio_put(led_red, true);
    else if (users == MAX_USUARIOS - 1)
        gpio_put(led_red, true), gpio_put(led_green, true);
    else if (users == MAX_USUARIOS - 2)
        gpio_put(led_green, true);
    else if (users == 0)
        gpio_put(led_blue, true); // Indica ambiente vazio
}

// --- Inicializa o buzzer ---
void buzzer_init()
{
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
}

// --- Toca nota no buzzer ---
void buzzer_play_note(int freq, int duration_ms)
{
    if (freq == REST)
    {
        gpio_put(BUZZER_PIN, 0);
        sleep_ms(duration_ms);
        return;
    }

    uint32_t period_us = 1000000 / freq;
    uint32_t cycles = (freq * duration_ms) / 1000;

    for (uint32_t i = 0; i < cycles; i++)
    {
        gpio_put(BUZZER_PIN, 1);
        sleep_us(period_us / 2);
        gpio_put(BUZZER_PIN, 0);
        sleep_us(period_us / 2);
    }
}


