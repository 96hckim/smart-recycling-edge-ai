# =====================================================================
# Makefile
# -----------------------------------------------------------------
# "make"만 치면 이 폴더의 모든 .c/.s 파일을 컴파일해서 최종 바이너리
# (rom_0x08000000.bin/.elf)를 만들어주는 빌드 스크립트.
# 보통 소스 코드(.c/.h)만 수정하면 되고, 이 파일은 툴체인 경로나
# 컴파일 옵션을 바꾸고 싶을 때만 건드리면 됨.
# =====================================================================

# Tool information
# ARM 크로스컴파일러 설치 경로
TOOL_DIR		= C:\arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi
VERSION			= 15.2.1
TARGET			= arm-none-eabi

# Project name & Link script
# 결과물 이름 (플래시 시작주소 0x08000000에서 따옴)
OUT_FILE_NAME 	= rom_0x08000000
# 링커 스크립트: 코드/변수를 메모리 어디에 배치할지 정의
LDS_FILE_NAME	= rom_0x08000000.lds

# Compiler & Linker Option: Cortex-M4 with Hardware FPU (FPv4-SP-D16 Hard-ABI)
CFLAGS  		= -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DSTM32F411xE
CFLAGS 			+= -std=gnu99 -O3 -Wall -g -c -fno-builtin -funsigned-char -fno-strict-aliasing -fno-common
LDFLAGS 		= -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
LDFLAGS    		+= --specs=nano.specs --specs=nosys.specs -u _printf_float -u _scanf_float -nostartfiles -ffreestanding
LDFLAGS    		+= -Wl,-Map=$(OUT_FILE_NAME).map -Wl,--cref -Wl,-EL -T $(LDS_FILE_NAME)

# Output File definition
# 보드에 실제로 굽는(flash) 순수 바이너리
OUT_BIN_FILE	= $(OUT_FILE_NAME).bin
# 디버깅 심볼 포함된 실행파일 (디버거/프로그래머가 사용)
OUT_ELF_FILE	= $(OUT_FILE_NAME).elf
# 각 함수/변수가 어느 주소에 배치됐는지 보여주는 맵 파일
OUT_MAP_FILE	= $(OUT_FILE_NAME).map
OUT_DUMP_FILE	= $(OUT_FILE_NAME).dmp

# Tool setting (크로스컴파일 툴체인의 각 실행파일 경로)
# 어셈블러 (crt0.s 컴파일용)
AS				= "$(TOOL_DIR)/bin/$(TARGET)-as"
# C 컴파일러
CC				= "$(TOOL_DIR)/bin/$(TARGET)-gcc"
# 링커
LD 				= "$(TOOL_DIR)/bin/$(TARGET)-ld"
# elf -> bin 변환
OBJCOPY			= "$(TOOL_DIR)/bin/$(TARGET)-objcopy"
# 디스어셈블(역어셈블) 덤프 생성
OBJDUMP			= "$(TOOL_DIR)/bin/$(TARGET)-objdump"

# Source & Header File
# 현재 폴더의 모든 .c 파일을 자동으로 찾아 포함
CSRC 			= $(wildcard *.c)
# 현재 폴더의 모든 .s(어셈블리) 파일
ASRC 			= $(wildcard *.s)
HEADER 			= $(wildcard *.h)
# 각 소스파일에 대응하는 .o(오브젝트) 파일 목록
OBJS    		= $(CSRC:.c=.o) $(ASRC:.s=.o)
DUMP 			= $(wildcard *.txt)

# Library and include folder
C_DIR			= $(TOOL_DIR)/$(TARGET)
GCC_DIR			= $(TOOL_DIR)/lib/gcc/$(TARGET)/$(VERSION)
# 표준 C 라이브러리(newlib) 링크
LIB_OPTION		= -L "$(C_DIR)/lib/thumb2" -L "$(GCC_DIR)/thumb2" -lc -lgcc
# 헤더 검색 경로
INCLUDE			= -nostdinc  -I. -I "$(C_DIR)/include" -I "$(GCC_DIR)/include"

# "make" 또는 "make all" -> 최종 .bin 파일까지 만든다 (기본 타겟)
all : $(OUT_BIN_FILE)

# .elf -> .bin으로 변환하고, 디버깅/분석용 역어셈블 텍스트(__dump.txt/__dump_all.txt)도 같이 생성
$(OUT_BIN_FILE): $(OUT_ELF_FILE)
	$(OBJCOPY) $(OUT_ELF_FILE) $(OUT_BIN_FILE) -O binary
	$(OBJDUMP) -x -D .\$(OUT_ELF_FILE) > .\__dump.txt
	$(OBJDUMP) -x -D -S .\$(OUT_ELF_FILE) > .\__dump_all.txt

# 모든 .o 파일을 링커 스크립트(LDS)에 따라 하나의 .elf 실행파일로 합침
$(OUT_FILE_NAME).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIB_OPTION) -o $@

# 소스(.c/.s) 또는 헤더(.h)가 바뀌면 전체를 다시 컴파일해서 .o 생성
# (파일별 개별 규칙이 아니라 뭉뚱그려 한 번에 컴파일하는 단순한 방식)
$(OBJS): $(ASRC) $(CSRC) $(HEADER)
	$(CC) $(ASRC) $(CSRC) $(INCLUDE) $(CFLAGS)

# "make clean" -> 빌드 결과물을 모두 삭제 (깨끗한 상태에서 다시 빌드하고 싶을 때)
clean :
	rm -f $(OUT_BIN_FILE)
	rm -f $(OUT_ELF_FILE)
	rm -f $(OUT_MAP_FILE)
	rm -f $(OBJS)
	rm -f $(DUMP)

# "make run" -> ST-Link(SWD) 프로그래머로 실제 보드에 .elf를 굽고 리셋까지 실행
run:
	STM32_Programmer_CLI.exe -c port=SWD -w ./rom_0x08000000.elf -v -rst -q