
; Function main
start main 2:
  entry:
    sub sp 12 64
    call sp 12 64 __spSub 12
    r1 = call func 1 2 3
    r2 = call func r1 2 3
    r3 = call func r1 r2 3
    r4 = call func r1 r2 r3
    store 4 r1 sp 0
    r1 = call func r2 r3 r4
    store 4 r1 sp 4
    r1 = load 4 sp 0
    store 4 r3 sp 8
    r3 = load 4 sp 4
    store 4 r4 sp 4
    r4 = call func r1 r2 r3
    store 4 r4 sp 0
    r4 = load 4 sp 4
    r3 = load 4 sp 8
    r1 = load 4 sp 0
    store 4 r3 sp 8
    r3 = call func r2 r4 r1
    store 4 r3 sp 0
    r3 = load 4 sp 8
    r1 = load 4 sp 0
    r2 = call func r1 r4 r3
    ret 0
end main
