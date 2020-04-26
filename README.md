
# 2020-1 SWPP Team 10 Course Project

This is Team 10's code repo of the course project for [Principles and Practices of Software Development](https://github.com/snu-sf-class/swpp202001) @ [Seoul National University](https://en.snu.ac.kr/). Project collaborators are Jasmine Abtahi ([YPH95](https://github.com/YPH95)), Alfiya Mussabekova ([alphy1](https://github.com/alphy1)), Hexiang Geng ([CuriousLocky](https://github.com/CuriousLocky)) and Minghang Li ([Matchy](https://github.com/matchy233)).

## Commit Message Conventions

There are two popular ways of writing a commit message: [Tim Pope style](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html) (This is also recommended in the official guideline of Git, [Pro Git](https://git-scm.com/book/en/v2/Distributed-Git-Contributing-to-a-Project)), and the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) style. The latter one is preferred in many large open-source projects since it dovetails [SemVer](https://semver.org/). Here we adopt the Tim Pope style, for it's succinctness.

Here are the 7 rules for writing a good Tim Pope style commit message:

1. Limit the subject line to 50 characters.
2. Capitalize *only* the first letter in the subject line.
3. *Don't* put a period at the end of the subject line.
4. Put a blank line between the subject line and the body.
5. Wrap the body at 72 characters.
6. Use the imperative mood, but not past tense.
7. Describe *what* was done and why, but not *how*.

Read [this nice blog post](https://chris.beams.io/posts/git-commit/) for explanation on why we set up those rules.

## Possible Compiler Improvement Ideas (~4.28)

Machine and ISA specifications can be found in `./resources/spec.pdf`

1. **`Shift` to `Mul`**:
	The machine we’re going to use has a greater performance penalty on shifting than multiplication (to be specific `mul` has cost of `0.6` but `lhr` has cost of `0.8`). So we can use multiplication/division to replace shifting. Similar optimization technique might also apply to optimizing `add/sub`, which has cost of `1.2`.

2. **Useless Instruction elimination**:
	Inspired by `gcc` high-level optimization. As our testing will only use `output()` and input() to interact with the user, there is a chance that some part of the code does not matter at all. If we can determine and eliminate that part of the code, we can potentially boost the performance.

3. **Branch merging**:
The `br` instruction takes 1 penalty even if it’s unconditional. Thus, we can do something on it like merging unconditional branches together, and eliminate some conditional one as well, for example, a condition is checked again in a branch where it’s already checked.

- Potential optimization details:
    - Unconditional br merging
    - Comparing two constants
    - Condition value double checked in branch
    - Known value propagation through and/or
 
4. **`inline` Function**:
If a function is not recursive, and not secretly recursive (function 1 calls function 2, and function 2 calls function 1), it’s possible to make it `inline` and save 2 penalties from call and ret.

5. **Lazy Allocation**:
Not sure if it’s helpful, but if we can delay the allocation (both on stack and heap) as much as possible, we can potentially save some memory head moving cost (since somehow the magical machine does not have random access memory).

6. **Stackify**: (Not sure if it works, or worth it, or helps)
If a memory space is from malloc, and it’s freed before the function returns, and the size is small, we can safely allocate the space on stack (though it shouldn’t be allocated on heap in the first place anyway).

7. **Constant propagation** Constant arithmatic will be resolved at compile time. We can save the result and substitute the arithmatic to reduce the cost.

### Related Links

[[Link to the Google Docs](https://docs.google.com/document/d/1EP3-g_DiDQYOyxL48RnxqTC8bqryYBrcjuHhQifear0/edit?usp=drivesdk)] [[Link to the Google Slides](https://docs.google.com/presentation/d/1FdUB4Q-jPUnIfqD3PuSsdiHp7DmbODjI-MP4H4iJER4/edit#slide=id.p)]


## TO-DOs

- [x] Add to-dos to track project progress status
- [x] Discuss project ideas
- [ ] Make presentation slides (due **April 28th**)
