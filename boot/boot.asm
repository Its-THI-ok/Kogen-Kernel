section .text
global _start

; Exporta a função externa em C (kmain)
extern kmain

; Ponto de Entrada do executavel do Kernel
_start:
    ; Chama a função externa em C
    call kmain
    ; Se a função terminar, cai pra baixo

; Fica em loop sem atividade => Stand by
.hang:
    jmp .hang