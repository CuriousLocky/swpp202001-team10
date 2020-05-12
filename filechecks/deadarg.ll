define internal i32 @f(i32 %x) {
; CHECK: start f 0:
    ret i32 0
}
; CHECK: end f

define i32 @f2(i32 %y) {
; CHECK: start f 1:
    %x = call i32 @f(i32 0)
    %ret = add i32 %x, %y
    ret i32 %ret
}
; CHECK: end f 2