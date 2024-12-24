/*
Microbenchmark for ECE552: Lab 2
Written by: ciricma1 and huynhra6
Compilation flag -O0 was used

Generated assembly via:
  /cad2/ece552f/compiler/bin/ssbig-na-sstrix-objdump mb.c -x -d -l mb_pisa > mb_disassemble.txt

Compiled for testing via:
  gcc -static -O0 -o mb mb.c
*/
#include <stdio.h>
#include <stdlib.h>

#define LOOP_COUNT 1000000

int main() {
    int a;
    int b;
    int i;

    // The main loop will not branch until the very last iteration, so only 1 misprediction should occur.
    for (i = 0; i < LOOP_COUNT; i++) {
        // 1 misprediction is caused every 8 iterations, since there are not enough history bits to store all possible unique histories for this branch.
        // If i starts at 0, the pattern will be NT,T,T,T,T,T,T,T. When the past 6 outcomes are TAKEN, the predictor will not be able to distinguish whether 
        // the next outcome will be T or NT, leading to a misprediction, assuming all predictions for all histories are initialized to WNT.
        if ((i % 8) == 0) {
            a = 5;
        }
        a = 15;
        /*  400260:	06 00 00 00 	bne $3,$0,400278 <main+0x88>         // If (i % 8) != 0, branch is TAKEN to 400278. This happens every 8 iterations.
            400268:	43 00 00 00 	addiu $2,$0,5                        
            400270:	34 00 00 00 	sw $2,16($30)                        // Set variable a to 5 (code executes when branch is NOT TAKEN)
            400278:	43 00 00 00 	addiu $2,$0,15                       // Code branches here if (i % 8) != 0
            400280:	34 00 00 00 	sw $2,16($30)                        // Set variable a to 15 (code executes always) */


        // The two-level predictor should be able to predict this branch successfully since the full pattern can fit within the available history bits.
        // However, 1 misprediction is caused every time for the 2-bit saturating counter predictor assuming the saturating counters are 
        // initially set to WNT, since the branch alternates between T/NT for each loop iteration.
        if ((i % 2) != 0) {
            b = 10;
        }
        b = 25;
        /*  400298:	05 00 00 00 	beq $3,$0,4002b0 <main+0xc0>         // If i is odd, branch is TAKEN to 4002b0. This happens every 2 iterations.
            4002a0:	43 00 00 00 	addiu $2,$0,10                 
            4002a8:	34 00 00 00 	sw $2,20($30)                        // Set variable b to 10 (code executes when branch is NOT TAKEN)
            4002b0:	43 00 00 00 	addiu $2,$0,25                       // Code branches here if i is even
            4002b8:	34 00 00 00 	sw $2,20($30)                        // Set variable a to 15 (code executes always) */
    }
    return 0;
}

/*
Full main() code snippet from objdump
-------------------------------------
004001f0 <main>:
main():
mb.c:17
  4001f0:	43 00 00 00 	addiu $29,$29,-40
  4001f4:	d8 ff 1d 1d 
  4001f8:	34 00 00 00 	sw $31,36($29)
  4001fc:	24 00 1f 1d 
  400200:	34 00 00 00 	sw $30,32($29)
  400204:	20 00 1e 1d 
  400208:	42 00 00 00 	addu $30,$0,$29
  40020c:	00 1e 1d 00 
  400210:	02 00 00 00 	jal 4004c8 <__main>
  400214:	32 01 10 00 
  400218:	34 00 00 00 	sw $0,24($30)
  40021c:	18 00 00 1e 
  400220:	28 00 00 00 	lw $2,24($30)
  400224:	18 00 02 1e 
  400228:	a2 00 00 00 	lui $3,15
  40022c:	0f 00 03 00 
  400230:	51 00 00 00 	ori $3,$3,16959
  400234:	3f 42 03 03 
  400238:	5b 00 00 00 	slt $2,$3,$2
  40023c:	00 02 02 03 
  400240:	05 00 00 00 	beq $2,$0,400250 <main+0x60>
  400244:	02 00 00 02 
  400248:	01 00 00 00 	j 4002e8 <main+0xf8>
  40024c:	ba 00 10 00 
  400250:	28 00 00 00 	lw $2,24($30)
  400254:	18 00 02 1e 
  400258:	4f 00 00 00 	andi $3,$2,7
  40025c:	07 00 03 02 
  400260:	06 00 00 00 	bne $3,$0,400278 <main+0x88>
  400264:	04 00 00 03 
  400268:	43 00 00 00 	addiu $2,$0,5
  40026c:	05 00 02 00 
  400270:	34 00 00 00 	sw $2,16($30)
  400274:	10 00 02 1e 
  400278:	43 00 00 00 	addiu $2,$0,15
  40027c:	0f 00 02 00 
  400280:	34 00 00 00 	sw $2,16($30)
  400284:	10 00 02 1e 
  400288:	28 00 00 00 	lw $2,24($30)
  40028c:	18 00 02 1e 
  400290:	4f 00 00 00 	andi $3,$2,1
  400294:	01 00 03 02 
  400298:	05 00 00 00 	beq $3,$0,4002b0 <main+0xc0>
  40029c:	04 00 00 03 
  4002a0:	43 00 00 00 	addiu $2,$0,10
  4002a4:	0a 00 02 00 
  4002a8:	34 00 00 00 	sw $2,20($30)
  4002ac:	14 00 02 1e 
  4002b0:	43 00 00 00 	addiu $2,$0,25
  4002b4:	19 00 02 00 
  4002b8:	34 00 00 00 	sw $2,20($30)
  4002bc:	14 00 02 1e 
  4002c0:	28 00 00 00 	lw $3,24($30)
  4002c4:	18 00 03 1e 
  4002c8:	43 00 00 00 	addiu $2,$3,1
  4002cc:	01 00 02 03 
  4002d0:	42 00 00 00 	addu $3,$0,$2
  4002d4:	00 03 02 00 
  4002d8:	34 00 00 00 	sw $3,24($30)
  4002dc:	18 00 03 1e 
  4002e0:	01 00 00 00 	j 400220 <main+0x30>
  4002e4:	88 00 10 00 
  4002e8:	42 00 00 00 	addu $2,$0,$0
  4002ec:	00 02 00 00 
  4002f0:	01 00 00 00 	j 4002f8 <main+0x108>
  4002f4:	be 00 10 00 
  4002f8:	42 00 00 00 	addu $29,$0,$30
  4002fc:	00 1d 1e 00 
  400300:	28 00 00 00 	lw $31,36($29)
  400304:	24 00 1f 1d 
  400308:	28 00 00 00 	lw $30,32($29)
  40030c:	20 00 1e 1d 
  400310:	43 00 00 00 	addiu $29,$29,40
  400314:	28 00 1d 1d 
  400318:	03 00 00 00 	jr $31
  40031c:	00 00 00 1f 
*/
