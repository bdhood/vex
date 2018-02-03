@echo off
tcc\tcc.exe assembler/assembler.c -lkernel32 -nostdlib -o assembler/assembler.exe
assembler\assembler.exe example.asm example.o
if %errorlevel% == 1 goto end
tcc\tcc.exe assembler/linker.c -lkernel32 -nostdlib -o assembler/linker.exe
assembler\linker.exe example.o -o code.bin
if %errorlevel% == 1 goto end
tcc\tcc.exe tools/codeattach/codeattach.c -lkernel32 -nostdlib -o tools/codeattach.exe
tools\codeattach.exe code.bin virtualcpu/virtualcpu.c virtualcpu/virtualcpu-loaded.c
if %errorlevel% == 1 goto end
tcc\tcc.exe virtualcpu/virtualcpu-loaded.c -lkernel32 -luser32 -nostdlib -o virtualcpu/vcpu.exe
virtualcpu\vcpu.exe
:end
pause
