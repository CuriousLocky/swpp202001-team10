
# 2020-1 SWPP Team 10 Course Project

This is Team 10's code repo of the course project for [Principles and Practices of Software Development](https://github.com/snu-sf-class/swpp202001) @ [Seoul National University](https://en.snu.ac.kr/). This project is an improved version of the naive compiler provided in [snu-sf-class/swpp202001-compiler](https://github.com/snu-sf-class/swpp202001-compiler), which converts LLVM IR to SWPP assembly.

Project collaborators are Jasmine Abtahi ([YPH95](https://github.com/YPH95)), Alfiya Mussabekova ([alphy1](https://github.com/alphy1)), Hexiang Geng ([CuriousLocky](https://github.com/CuriousLocky)) and Minghang Li ([Matchy](https://github.com/matchy233)).

## Build and Run the project

### How to compile

To compile this project, you'll need to clone & build LLVM 10.0 first. Please follow README.md from https://github.com/aqjune/llvmscript using llvm-10.0.json.

After LLVM 10.0 is successfully built, please run:

```bash
./configure.sh <LLVM bin dir (ex: ~/llvm-10.0-releaseassert/bin)>
make
```

To run tests including FileCheck and Google Test:

```bash
make test
```

### How to run

Compile LLVM IR `input.ll` into an assembly `a.s` using this command:

```bash
./sf-compiler input.ll -o a.s
```

To see the IR that has registers depromoted before emitting assembly, please run:

```bash
./sf-compiler input.ll -o a.s -print-depromoted-module
```

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

## Possible Compiler Improvement Ideas

See [project/optList.md](project/spec.pdf) for possible optimization opitions.


## TO-DOs

- [x] Add to-dos to track project progress status -- Matchy
- [x] Discuss project ideas -- All
- [x] Make presentation slides (due **April 28th**) -- All
- [x] Wrtie documentation (due **May 2nd**) -- Matchy
- [ ] Learn Travis-CI -- Matchy
- [ ] Learn how to add badges to README.md -- Matchy
