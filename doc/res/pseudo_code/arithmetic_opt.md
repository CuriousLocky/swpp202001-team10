# A: Requirment and Specification 

## Optimization 1.

### Arithmetic Optimazations: 
Some of the Arithmetic Instructions in our machine are costier than the other. By replcaing these instrucations using matchers, we can save the cost up to %50. We are going to have a set of pattern instructions with corresponding replacable instructions.
| Instruction   |      Replacable Instruction     |   Saved Cost |
|----------|:-------------:|------:|
| ashr |  udiv | %25 |
| shl |    mul   |  %25  |
| add  | mul |    %50 |
| sub | mul |    %50 |

### Pseudocode:
```
FOR each use of Instruction i in input.ll 
   IF Instruction i matches with Pattern p at index j
     replace the use with replacable Instruction r at index j
```
### IR program optimizations, before and after:
Case 1: shift to mul/div 
``` llvm
%1 = shl  %a, 1, 32 ;before
%1 = mul  %a, 2, 32 ;after
%2 = ashr %b, 1, 32 ;before
%2 = udiv %b, 2, 32 ;after
```
Case 2: add/sub val with val's multiplicands to mul

``` llvm
%1 = add  %a, %a, 32 ;before
%1 = mul  %a, 2,  32 ;after
%2 = sub  %b, %b, 32 ;before
%2 = mul  %b, 0,  32 ;after
```
Case 3: add/sub with zero to mul

``` llvm
%1 = add  %a, 0,  32 ;before
%1 = mul  %a, 1,  32 ;after
%2 = sub  %b, 0,  32 ;before
%2 = mul  %b, 1,  32 ;after
%3 = sub   0, %c, 32 ;before
%3 = mul  -1, %c, 32 ;after
```
