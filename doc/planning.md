
This is the tentative planning for Team 10. Note that this is subject to changes.

# Table of Contents

- [Table of Contents](#table-of-contents)
  - [Sprint 1](#sprint-1)
    - [About Existing LLVM Optimization Integration](#about-existing-llvm-optimization-integration)
  - [Sprint 2](#sprint-2)
    - [About Implementation of New Backend](#about-implementation-of-new-backend)
    - [About Existing LLVM Optimization Integration](#about-existing-llvm-optimization-integration-1)
  - [Sprint 3](#sprint-3)
    - [About Lacking Integration of Existing LLVM Optimization](#about-lacking-integration-of-existing-llvm-optimization)
  - [Wrap-up Period](#wrap-up-period)

## Sprint 1

| Optimizations | Responsable Team Member |
|:---|:---|
| Arithematic optimization | Jasmine Abtahi Yasaman |
| Reset insertion | Hexiang Geng |
| `malloc` to `alloca` | Alfiya Mussabecova |
| Existing LLVM optimization integration | Minghang Li |

### About Existing LLVM Optimization Integration

The existing LLVM optimization that will be integrated are:

- Function inlining (`llvm/Transforms/IPO/Inliner.h`)
- Constant folding (`llvm/Tranforms/Scalalr/GVN.h`)
- Dead argument elimination (`llvm/Transforms/IPO/DeadArgumentElimination.h`)

## Sprint 2


| Optimizations | Responsable Team Member |
|:---|:---|
| Implementation of new backend | Hexiang Geng |
| Implementation of new backend | Minghang Li |
| `malloc` to `alloca` (continued) | Alfiya Mussabecova |
| Existing LLVM optimization integration | Jasmine Abtahi Yasaman |

### About Implementation of New Backend

Due to various limitations of the `SimpleBackend` and `AssemblyEmitter`, some of the optimizations are difficult to implement. After careful consideration we decided to implement a new backend named `LessSimpleBackend` along with its corresponding assembly emitter named `NewAssemblyEmitter`.

The new backend will enable the following optimizations:

- Reordering memory accesses (support reset heap / reset stack)
- Register allocation

### About Existing LLVM Optimization Integration

The existing LLVM optimization that will be integrated are:

- Dead code elimination (`llvm/Transforms/Scalar/DCE.h`)
- Lowering `alloca`s to IR `registers` (`llvm/Transforms/Utils/PromoteMemToReg.h`)
- Delete unused `global` variables (`llvm/Transforms/IPO/GlobalOpt.h`)

## Sprint 3

| Optimizations | Responsable Team Member |
|:---|:---|
| Implementation of new backend (continued) | Hexiang Geng |
| Implementation of new backend (continued) | Minghang Li |
| Optimize order of applying passes | Jasmine Abtahi Yasaman |
| `malloc` to `alloca` (continued) | Alfiya Mussabecova |

### About Lacking Integration of Existing LLVM Optimization

According to the original plan, Alfiya Mussabecova was responsible for integrating existing LLVM optimization in this Sprint. However, due to the lag in her implementation of `malloc` to `alloca`, she could not start to do the integration.

## Wrap-up Period

| Optimizations | Responsable Team Member |
|:---|:---|
| Refinement of new backend | Hexiang Geng |
| Refinement of new backend | Minghang Li |
| Existing LLVM optimization integration | Minghang Li, Jasmine Abtahi Yasaman |
| Finish `malloc` to `alloca` (if possible) | All team members |

Since this is not a Sprint, the limitation on integrating existing LLVM optimization is assumed to be removed. We'll try to integrate as many existing LLVM optimizations as possible. Most likely we'll integrate the following passes:

- Loop Interchange
- `br` to `switch`
- Tail call elimination
