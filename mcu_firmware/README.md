# mcu_firmware

Smart Recycling Edge AI 프로젝트의 MCU(STM32F411) 펌웨어입니다.
Jetson(영상 분류 보드)이 판별한 재활용품 종류를 UART로 받아 게이트/분류 서보를 구동하고, 초음파 센서로 각 수거함의 적재율을 측정해 다시 UART로 보고합니다.

## 하드웨어

| 항목 | 내용 |
|---|---|
| MCU | STM32F411xE (Cortex-M4, Hardware FPU) |
| 통신 | USART2 (PA2/PA3, 115200bps) — Jetson/터미널과의 실통신 채널. USART1은 예비/디버깅용 |
| 서보 | 4채널 PWM (TIM3, 20ms/50Hz 주기, 0.5~2.5ms 펄스) |
| 초음파 센서 | 4채널 (HC-SR04류), GPIOC Trig/Echo, TIM4(1us 카운터) 공유 순차 측정 |

**서보 채널 배정**

| 채널 | 역할 |
|---|---|
| CH0 | 게이트(문) 개폐 |
| CH1, CH2 | 분류 모터 — 두 채널의 LEFT/RIGHT 조합(2비트)으로 PET/CAN/PAPER/VINYL 4방향 표현 |
| CH3 | 미사용 |

**초음파 채널 → 수거함**

| 채널 | 수거함 |
|---|---|
| CH0 | PAPER |
| CH1 | CAN |
| CH2 | PET |
| CH3 | VINYL |

## 빌드 & 플래시

ARM GNU 툴체인(`arm-none-eabi-gcc`)이 필요하며, 경로는 `Makefile`의 `TOOL_DIR`에서 지정합니다.

```sh
make        # rom_0x08000000.bin / .elf 생성
make clean  # 빌드 산출물 삭제
make run    # ST-Link(SWD)로 보드에 플래시 후 리셋
```

## 파일 구성

| 파일 | 역할 |
|---|---|
| `main.c` | 메인 루프(super loop) — UART 명령 처리, 서보 업데이트, 자동 닫힘 체크, 주기 보고 |
| `isr.c` | 인터럽트 핸들러 (SysTick, USART2 RX, 미등록 인터럽트 안전장치) |
| `uart.c` | USART1/2 초기화 및 송수신 |
| `ultrasonic.c` | 초음파 센서 4채널 거리 측정 |
| `servo4.c` | 서보 4채널 각도/속도 제어 (PWM 펄스 변환 + 램프) |
| `timer.c` | SysTick(1ms 틱), TIM3 PWM 초기화 |
| `recycle.c` | 게이트/분류 서보를 묶은 도어 제어 상태머신, 자동 닫힘 로직 |
| `clock.c`, `system_stm32f4xx.c` | 시스템 클럭(PLL) 설정 |
| `syscalls.c` | newlib 재타겟(printf 등을 UART로 연결) |
| `device_driver.h`, `macro.h`, `option.h` | 공통 include, 레지스터 조작 매크로, 보드 설정값 |
| `rom_0x08000000.lds` | 링커 스크립트 (플래시 시작주소 0x08000000) |

## 동작 흐름

1. **부팅**: FPU 활성화 → 클럭 설정 → UART2 초기화 → 타이머(SysTick/PWM) 초기화 → 초음파 GPIO/TIM4 초기화 → 서보 중앙값 정렬 → UART RX 인터럽트 활성화
2. **메인 루프**가 매 반복마다 확인하는 것:
   - UART로 새 줄이 들어왔는지 (`$`로 시작하면 Jetson 프로토콜, 아니면 사람이 보내는 서보 테스트 명령)
   - 서보 램프 진행 (`Servo4_Update`)
   - 문이 열려 있으면 150ms마다 해당 수거함 거리 체크 → 자동 닫힘 조건 판정
   - 1초마다 문 상태 + 4개 수거함 적재율 보고

### 초음파 거리 측정

Trig 10us 펄스 발사 → Echo가 HIGH인 시간(us) 측정 → `거리(cm) = pulse_us / 58` (음속 340m/s 기준 왕복거리 환산). 무응답(10ms 초과) 또는 사거리 초과(30ms 초과, 약 5m)면 `-1.0f` 반환.

### 적재율(%) 계산

바닥까지 거리를 캘리브레이션된 두 기준값으로 환산합니다.

```
BIN_EMPTY_CM = 30   // 빈 통일 때 거리
BIN_FULL_CM  = 5    // 꽉 찬 통일 때 거리

fill% = clamp((BIN_EMPTY_CM - dist) / (BIN_EMPTY_CM - BIN_FULL_CM) * 100, 0, 100)
```

### 문 자동 닫힘 (3중 안전장치)

| 조건 | 값 | 목적 |
|---|---|---|
| 최소 개방 시간 | 2000ms | 열자마자 "비었음"으로 오판해 바로 닫히는 것 방지 |
| 비움 디바운스 | 1200ms | 거리 ≥ 25cm(비어 보임) 상태가 이 시간 이상 유지돼야 실제로 닫음 (센서 노이즈 필터) |
| 최대 개방 시간 | 10000ms | 방치 시 무한정 열려있지 않도록 하는 failsafe |

## UART 프로토콜

**입력 (MCU가 받는 명령)**

| 명령 | 형식 | 설명 |
|---|---|---|
| 서보 테스트 (사람용) | `<servo 1~3> <angle 0~180> [speed deg/s]` | 예: `1 90`, `1 90 30`. speed 생략 시 즉시 이동 |
| 도어 오픈 (Jetson용) | `$DOOR_OPEN:<PET\|CAN\|PAPER\|VINYL>` | 분류 서보 위치 후 300ms 뒤 게이트 오픈 |
| 도어 클로즈 (Jetson용) | `$DOOR_CLOSE` | 무시됨 — 닫힘은 항상 보드가 자동 판단 |

**출력 (MCU가 보내는 상태)**

| 메시지 | 형식 | 전송 시점 |
|---|---|---|
| 문 상태 | `$DOOR_STATE:OPEN` / `$DOOR_STATE:CLOSED` | 상태 변화 시 + 1초 주기 |
| 적재율 | `$BIN:paper/can/pet/vinyl` (0~100) | 1초 주기 |
