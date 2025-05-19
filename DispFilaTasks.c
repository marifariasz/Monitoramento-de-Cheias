/*
 *  Por: Wilton Lacerda Silva
 *  Data: 10/05/2025
 *
 *  Exemplo do uso de Filas queue no FreeRTOS com Raspberry Pi Pico
 *
 *  Descrição: Leitura do valor do joystick e exibição no display OLED SSD1306
 *  com comunicação I2C. O valor do joystick é lido a cada 100ms e enviado para a fila.
 *  A task de exibição recebe os dados da fila e atualiza o display a cada 100ms.
 *  Os leds são controlados por PWM, com brilho proporcional ao desvio do joystick.
 *  O led verde controla o eixo X e o led azul controla o eixo Y.
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>             // Funções padrão de entrada/saída
#include "hardware/pwm.h"      // Modulação por largura de pulso (PWM) para buzzer
#include "hardware/clocks.h"   // Controle de clocks do sistema
#include "ws2818b.pio.h"       // Programa PIO para controlar LEDs WS2812B
#include "hardware/pio.h"      // Interface para Programmable I/O (PIO)

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_BLUE 12
#define LED_GREEN  11
#define LED_RED 13
#define BUZZER 21
#define LED_PIN 7              // Pino para matriz de LEDs WS2812B
uint sm;                           // Máquina de estado do PIO para LEDs
#define LED_COUNT 25               // Número total de LEDs na matriz 5x5



// Estrutura para representar um pixel RGB na matriz de LEDs
struct pixel_t {
    uint8_t G, R, B;           // Componentes de cor: verde, vermelho, azul
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;       // Tipo para LEDs NeoPixel (WS2812B)

// Variáveis globais para controle da matriz de LEDs
npLED_t leds[LED_COUNT];       // Array que armazena o estado de cada LED
PIO np_pio;                    // Instância do PIO para controlar a matriz
void npDisplayDigit(int digit);
// Matrizes que definem os padrões de exibição na matriz de LEDs (5x5 pixels)
const uint8_t digits[4][5][5][3] = {
    // Situação 1: sinal verde, seta pra cima
    {
        {{0, 0, 0}, {0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 100, 0}, {0, 0, 0}},
        {{0, 100, 0}, {0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 100, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 100, 0}, {0, 0, 0}},
        {{0, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 100, 0}}
    },
    // Situação 2: sinal amarelo, sinal de exclamação
    {
        {{0, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    },
    // Situação 3: sinal vermelho, um X
    {
        {{100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {100, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {100, 0, 0}, {0, 0, 0}},
        {{100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {100, 0, 0}}
    }
};

// Define as cores de um LED na matriz
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) {
    leds[index].R = r; // Componente vermelho
    leds[index].G = g; // Componente verde
    leds[index].B = b; // Componente azul
}

// Limpa a matriz de LEDs, exibindo o padrão de dígito 4 (padrão para limpar)
void npClear() {
    npDisplayDigit(4);
}

// Inicializa a matriz de LEDs WS2812B usando o PIO
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program); // Carrega programa PIO
    np_pio = pio0; // Usa PIO0
    sm = pio_claim_unused_sm(np_pio, true); // Reserva uma máquina de estado
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f); // Inicializa PIO
    npClear(); // Limpa a matriz ao inicializar
}

// Escreve os dados dos LEDs na matriz
void npWrite() {
    for (uint i = 0; i < LED_COUNT; i++) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G); // Envia componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R); // Envia componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B); // Envia componente azul
    }
    sleep_us(100); // Pequeno atraso para estabilizar a comunicação
}

// Calcula o índice de um LED na matriz com base nas coordenadas (x, y)
int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24 - (y * 5 + x); // Linhas pares: ordem direta
    } else {
        return 24 - (y * 5 + (4 - x)); // Linhas ímpares: ordem invertida
    }
}

// Exibe um dígito ou padrão na matriz de LEDs
void npDisplayDigit(int digit) {
    for (int coluna = 0; coluna < 5; coluna++) {
        for (int linha = 0; linha < 5; linha++) {
            int posicao = getIndex(linha, coluna); // Calcula índice do LED
            npSetLED(posicao, digits[digit][coluna][linha][0], // Componente R
                              digits[digit][coluna][linha][1], // Componente G
                              digits[digit][coluna][linha][2]); // Componente B
        }
    }
    npWrite(); // Atualiza a matriz com os novos dados
}



typedef struct
{
    uint16_t x_pos;
    uint16_t y_pos;
} joystick_data_t;

QueueHandle_t xQueueJoystickData;

void vJoystickTask(void *params)
{
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_init();

    joystick_data_t joydata;

    while (true)
    {
        adc_select_input(0); // GPIO 26 = ADC0
        joydata.y_pos = adc_read();

        adc_select_input(1); // GPIO 27 = ADC1
        joydata.x_pos = adc_read();

        xQueueSend(xQueueJoystickData, &joydata, 0); // Envia o valor do joystick para a fila
        vTaskDelay(pdMS_TO_TICKS(100));              // 10 Hz de leitura
    }
}

void vDisplayTask(void *params)
{
    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o display SSD1306
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    joystick_data_t joydata;
    char buffer[32]; // Buffer maior para segurança
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            uint16_t per_x = (joydata.x_pos * 100) / 4095;
            uint16_t per_y = (joydata.y_pos * 100) / 4095;


            if(joydata.x_pos >= 2866 && joydata.y_pos < 3276){

                // Limpa o display
                ssd1306_fill(&ssd, false);
                ssd1306_draw_string(&ssd, "ATENCAO", 30, 8); // Rótulo na linha 1
                // Exibe Nível da água (rótulo e valor em linhas separadas)
                ssd1306_draw_string(&ssd, "Nivel da agua", 15, 30); // Rótulo na linha 1
                snprintf(buffer, sizeof(buffer), "%u%%", per_x);
                ssd1306_draw_string(&ssd, buffer, 50, 42); // Valor na linha 2
                // Envia os dados para o display
                ssd1306_send_data(&ssd);
            }else if(joydata.x_pos < 2866 && joydata.y_pos >= 3276){
                // Limpa o display
                ssd1306_fill(&ssd, false);
                ssd1306_draw_string(&ssd, "ATENCAO", 35, 8); // Rótulo na linha 1
                // Exibe Volume de chuva (rótulo e valor em linhas separadas)
                ssd1306_draw_string(&ssd, "Volume de chuva", 4, 30); // Rótulo na linha 3
                snprintf(buffer, sizeof(buffer), "%u%%", per_y);
                ssd1306_draw_string(&ssd, buffer, 50, 42); // Valor na linha 4
                // Envia os dados para o display
                ssd1306_send_data(&ssd);
            }else if(joydata.x_pos >= 2866 && joydata.y_pos >= 3276){
                // Limpa o display
                ssd1306_fill(&ssd, false);
                // Exibe as strings centralizadas com posições x calculadas manualmente
                ssd1306_draw_string(&ssd, "ATENCAO", 35, 5); // Rótulo na linha 1

                // Nível da água
                ssd1306_draw_string(&ssd, "Nivel da agua", 15, 20); // Rótulo na linha 1
                snprintf(buffer, sizeof(buffer), "%u%%", per_x);
                ssd1306_draw_string(&ssd, buffer, 52, 28); // Valor na linha 2

                // Volume de chuva
                ssd1306_draw_string(&ssd, "Volume de chuva", 4, 40); // Rótulo na linha 3
                snprintf(buffer, sizeof(buffer), "%u%%", per_y);
                ssd1306_draw_string(&ssd, buffer, 52, 48); // Valor na linha 4
                // Envia os dados para o display
                ssd1306_send_data(&ssd);
            }
            else{

                // Limpa o display
                ssd1306_fill(&ssd, false);

                // Exibe Nível da água (rótulo e valor em linhas separadas)
                ssd1306_draw_string(&ssd, "Nivel da agua", 15, 8); // Rótulo na linha 1
                snprintf(buffer, sizeof(buffer), "%u%%", per_x);
                ssd1306_draw_string(&ssd, buffer, 50, 20); // Valor na linha 2

                // Exibe Volume de chuva (rótulo e valor em linhas separadas)
                ssd1306_draw_string(&ssd, "Volume de chuva", 4, 30); // Rótulo na linha 3
                snprintf(buffer, sizeof(buffer), "%u%%", per_y);
                ssd1306_draw_string(&ssd, buffer, 50, 42); // Valor na linha 4

                // Envia os dados para o display
                ssd1306_send_data(&ssd);
            }

            // Atraso para evitar atualização excessiva
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vLedRGBTask(void *params)
{
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            if(joydata.x_pos >= 2866 || joydata.y_pos >= 3276){
                gpio_put(LED_RED, 1);
                gpio_put(LED_GREEN, 0);
            }
            else{
                gpio_put(LED_RED, 0);
                gpio_put(LED_GREEN, 1);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}

// Toca um som no buzzer com frequência e duração especificadas
void play_buzzer(uint pin, uint frequency, uint duration_ms) {
    gpio_set_function(pin, GPIO_FUNC_PWM); // Configura o pino como PWM
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice PWM
    pwm_config config = pwm_get_default_config(); // Carrega configuração padrão
    // Ajusta o divisor de clock para atingir a frequência desejada
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (frequency * 4096));
    pwm_init(slice_num, &config, true); // Inicializa o PWM
    pwm_set_gpio_level(pin, 2048); // Define duty cycle (~50%)
    vTaskDelay(pdMS_TO_TICKS(duration_ms)); // Aguarda a duração do som
    pwm_set_gpio_level(pin, 0); // Desliga o buzzer
    pwm_set_enabled(slice_num, false); // Desativa o PWM
}


void vBuzzerTask(void *params)
{
    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);

    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            if(joydata.x_pos >= 2866 && joydata.y_pos < 3276){
                play_buzzer(BUZZER, 2000, 100);
            }else if(joydata.x_pos < 2866 && joydata.y_pos >= 3276){
                play_buzzer(BUZZER, 3000, 100);
            }
            else if(joydata.x_pos >= 2866 && joydata.y_pos >= 3276){
                play_buzzer(BUZZER, 4000, 200);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}

void vMatrizTask(void *params){
    npInit(LED_PIN);
    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            if(joydata.x_pos >= 2866 || joydata.y_pos >= 3276){
                npDisplayDigit(1);
            }else{
                npClear();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
    npDisplayDigit(2);
}
// Modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();

    // Cria a fila para compartilhamento de valor do joystick
    xQueueJoystickData = xQueueCreate(5, sizeof(joystick_data_t));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 512, NULL, 1, NULL);
    xTaskCreate(vLedRGBTask, "LED red Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrizTask, "Matriz Task", 256, NULL, 1, NULL);
    
    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}
