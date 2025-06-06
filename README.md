# Painel de Controle Interativo com FreeRTOS - BitDogLab (RP2040)

Este projeto simula o **acesso de alunos a um laboratório**, utilizando a placa **BitDogLab com o microcontrolador RP2040**, combinando hardware embarcado e multitarefa com **FreeRTOS**. O sistema monitora e controla o limite de usuários simultâneos, oferecendo um painel visual e sonoro em tempo real.

---

## 📌 Funcionalidades

- ✅ Simulação de **entrada e saída de alunos** com controle de lotação
- ✅ **Display OLED** (SSD1306) via I2C mostrando status e ocupação do laboratório
- ✅ **LEDs indicadores**:
  - 🔴 Lotado
  - 🟡 Quase cheio
  - 🟢 Disponível
  - 🔵 Vazio
- ✅ **Buzzer** com sons distintos para entrada, saída, erro e reset
- ✅ Botões com **interrupções e debounce**
- ✅ Modo de **reset geral** via botão do joystick
- ✅ Implementação de **3 tarefas simultâneas** usando FreeRTOS:
  - `vTaskEntrada`
  - `vTaskSaida`
  - `vTaskReset`

---

## 🧠 Tecnologias e Ferramentas

- **FreeRTOS** — Sistema operacional em tempo real
- **Pico-SDK** — SDK oficial do RP2040
- **Display OLED SSD1306** — Comunicação via I2C
- **Semáforos binários e de contagem**, além de **mutex**
- **Debounce com temporização** em interrupções
- **Controle de buzzer** com geração de notas musicais

---

## ⚙️ Hardware Utilizado

- Placa **BitDogLab** com **RP2040**
- Display OLED **SSD1306** (conectado via I2C)
- **Botões físicos** (Entrada, Saída e Reset via Joystick)
- **LEDs RGB** (GPIOs 11, 12, 13)
-  **Buzzer** (GPIO 10)

---

## 📷 Interface no Display

O display OLED mostra:

![WhatsApp Image 2025-05-25 at 23 44 03](https://github.com/user-attachments/assets/dc5ba17e-de54-4d9e-8c18-5db17544c439)



