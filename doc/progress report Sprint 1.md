# Progress report for Sprint 1

The optimization implementated in Sprint 1 are:
1. Arithmetic optimization
2. Malloc to alloca
3. Insert reset instruction
4. Import existing passes

The detailed progress of each optimization are as follow.

## Arithmetic optimization

### Development state
### Test result
### Comments

## Malloc to alloca

### Development state
### Future plan
### Coments

## Insert reset instruction

### Development state

The reset instruction is added by modifying the backend code. If an instruction
accesses memory, and another following instruction accesses another position, a
reset is inserted before the second one. 

The optimization is completed, but it does not affect the output assembly. After
brief analysis of the backend code, the reason is found. The current simple backend
will put all local variables on stack first, and all of the registers are used as
bridges. This means all of real registers used are from alloca, which is Stack.

Because of this reason, all of the instructions accessing memory are on Stack. So 
this optimization will never be triggered. 

### IR program for testing

No IR program is provided since this optimization is rather low-level and tightly
bind to the backend. Due to the reason mentioned above, the generated assembly 
is also not changed with this optimization applied. In this case, there is no point
to have IR program specially for this optimization.

The test from TA are still tested to show no effects are made.

### Test result

All of the test results are in reset_opt_test.log. Some minor differences can be
seen. They can be from this optimization but they are more likely to be the error 
in given test traces. Before adding this optimization, there were also some
differences in the costs and max heap usages.

### Future plan

It's obvious that the current backend is limiting the performance. The reset will
not work if the backend is not changed. Thus, it's planned that in next sprint
the backend code be reworked, and most of the local variables should be put on 
registers after the rework. At that point, the reset can be truly put in use.

### Comments

Current implementation of passing reset to the assembly emitter is by creating
dummy function calls. Two unique function names are generated, and are used for 
telling the emitter to insert a reset. This is not the most elegant way but is 
convenient to use. I'm starting to have an evil thought to use it more in the
reworked backend.

## Import existing passes

