# A: Requirment and Specification 

## Optimization 1.

### Arithmetic Optimazations: 
Some of the Arithmetic Instructions in our machine are costier than the other. By replcaing these instrucations using matchers, we can save the cost up to %50. 

### Pseudocode:
```
FOR each use of Instruction i in input.ll 
   IF Instruction i matches with Pattern p
     replace the use with replacable Instruction r 
```


### IR program before optimization: 


