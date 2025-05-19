# Estação de Alerta de Enchente 🚨

![Raspberry Pi Pico](https://img.shields.io/badge/Raspberry%20Pi-Pico-blue) ![FreeRTOS](https://img.shields.io/badge/FreeRTOS-v10.4.3-green) ![License](https://img.shields.io/badge/License-Educational-orange)

## 🎯 Objetivo Geral
A estação de alerta de enchente é um sistema embarcado que simula uma estação de monitoramento de enchentes usando a **Raspberry Pi Pico (RP2040)** e **FreeRTOS** 🌟. Ele lê dados de um joystick analógico para simular níveis de água e volume de chuva, exibindo alertas visuais em um display OLED SSD1306, LEDs RGB, e uma matriz de LEDs WS2812B, além de alertas sonoros via buzzer 🎶. A comunicação entre tarefas é feita exclusivamente por filas, garantindo modularidade e eficiência ⚙️.

---

## 📋 Descrição Funcional
O sistema interpreta os eixos X e Y do joystick como "Nível da água" e "Volume de chuva" (0-100%) e processa os dados em várias tarefas do FreeRTOS. Veja como funciona:

### 🕹️ Leitura do Joystick
- **Tarefa**: `vJoystickTask`
- Lê os eixos X (GPIO 26, ADC0) e Y (GPIO 27, ADC1) a cada 100ms.
- Envia os dados para a fila `xQueueJoystickData`.

### 📺 Display OLED SSD1306
- **Tarefa**: `vDisplayTask`
- Exibe informações no display (128x64, I2C, endereço 0x3C).
- **Condições de Exibição**:
  | Condição | Eixo X | Eixo Y | Exibição no Display |
  |----------|--------|--------|---------------------|
  | 1️⃣ | ≥ 2866 | < 3276 | "ATENÇÃO" + "Nível da água" (%). |
  | 2️⃣ | < 2866 | ≥ 3276 | "ATENÇÃO" + "Volume de chuva" (%). |
  | 3️⃣ | ≥ 2866 | ≥ 3276 | "ATENÇÃO", "Nível da água" e "Volume de chuva". |
  | 🟢 Normal | Outros | Outros | "Nível da água" e "Volume de chuva" sem alerta. |

### 💡 LEDs RGB
- **Tarefa**: `vLedRGBTask`
- Controla LEDs nos GPIOs 11 (verde), 12 (azul), e 13 (vermelho).
- **Lógica**:
  - 🟥 LED vermelho: X ≥ 2866 ou Y ≥ 3276 (alerta).
  - 🟩 LED verde: Condição normal.

### 🖼️ Matriz de LEDs WS2812B
- **Tarefa**: `vMatrizTask`
- Controla uma matriz 5x5 (GPIO 7) via PIO.
- **Padrões Visuais**:
  | Padrão | Descrição | Condição |
  |--------|-----------|----------|
  | ❗ Exclamação Amarela | Alerta | X ≥ 2866 ou Y ≥ 3276 |

### 🎵 Buzzer
- **Tarefa**: `vBuzzerTask`
- Emite sons no GPIO 21 via PWM.
- **Frequências**:
  | Condição | Frequência | Duração |
  |----------|------------|---------|
  | 1️⃣ | 2000 Hz | 100ms |
  | 2️⃣ | 3000 Hz | 100ms |
  | 3️⃣ | 4000 Hz | 200ms |

### 🔄 Modo BOOTSEL
- Botão no GPIO 6 entra no modo bootloader da Pico.

---

## 🛠️ Periféricos e Código
### 🔌 Periféricos da BitDogLab/RP2040
| Periférico | Função | Pinos |
|------------|--------|-------|
| **ADC** | Lê joystick (12 bits) | GPIO 26 (X), GPIO 27 (Y) |
| **I2C** | Comunica com OLED | GPIO 14 (SDA), GPIO 15 (SCL) |
| **GPIO** | LEDs RGB, botão | GPIO 11, 12, 13, 6 |
| **PWM** | Som no buzzer | GPIO 21 |
| **PIO** | Controla WS2812B | GPIO 7 |

---

## 📦 Dependências
- **FreeRTOS** 🕒: Gerenciamento de tarefas e filas.
- **Pico SDK** 🛠️: Programação da RP2040.
- **Bibliotecas** 📚:
  - `ssd1306.h`, `font.h`: Controle do OLED.
  - `ws2818b.pio`: Controle da matriz WS2812B.
- **Ferramentas** 🔧:
  - GCC para ARM.
  - CMake.
  - Picotool ou modo BOOTSEL.

---

## ⚙️ Configuração do Ambiente
1. **Instale o Pico SDK** 📥:
   - Configure o `PICO_SDK_PATH` ([Pico SDK](https://github.com/raspberrypi/pico-sdk)).
2. **Adicione Bibliotecas** 📂:
   - Copie `ssd1306.h`, `font.h`, `ws2818b.pio` para o projeto.
3. **CMakeLists.txt** 📝:
   ```cmake
   cmake_minimum_required(VERSION 3.13)
   include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
   project(floodguard C CXX ASM)
   set(CMAKE_C_STANDARD 11)
   set(CMAKE_CXX_STANDARD 17)
   pico_sdk_init()
   add_executable(floodguard main.c)
   target_include_directories(floodguard PRIVATE ${CMAKE_CURRENT_LIST_DIR})
   target_link_libraries(floodguard pico_stdlib hardware_gpio hardware_adc hardware_i2c hardware_pwm hardware_pio hardware_clocks freertos)
   pico_add_extra_outputs(floodguard)
   ```
4. **Compile e Carregue** 🚀:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
   - Copie o `.uf2` para a Pico no modo BOOTSEL.

---

## 🔗 Conexões do Hardware
| Componente            | Pino da Pico       | Emoji |
|-----------------------|--------------------|-------|
| Joystick X            | GPIO 26 (ADC0)     | 🕹️ |
| Joystick Y            | GPIO 27 (ADC1)     | 🕹️ |
| OLED SDA              | GPIO 14            | 📡 |
| OLED SCL              | GPIO 15            | 📡 |
| LED Verde             | GPIO 11            | 🟢 |
| LED Azul              | GPIO 12            | 🔵 |
| LED Vermelho          | GPIO 13            | 🔴 |
| Buzzer                | GPIO 21            | 🎶 |
| Matriz WS2812B        | GPIO 7             | 🖼️ |
| Botão BOOTSEL         | GPIO 6             | 🔄 |

---

## 🚀 Como Usar
1. **Carregue o Firmware** 💾: Use o modo BOOTSEL.
2. **Interaja com o Joystick** 🕹️:
   - Mova o joystick para simular "Nível da água" (X) e "Volume de chuva" (Y).
   - **Condição 1**: Alto nível → 🔴 LED vermelho, 🎶 2000 Hz, ❗ Exclamação.
   - **Condição 2**: Alto volume → 🔴 LED vermelho, 🎶 3000 Hz, ❗ Exclamação.
   - **Condição 3**: Ambos altos → 🔴 LED vermelho, 🎶 4000 Hz, ❗ Exclamação.
   - **Normal**: 🟢 LED verde, matriz limpa.
3. **Modo BOOTSEL** 🔄: Pressione o botão no GPIO 6.                         |

---
