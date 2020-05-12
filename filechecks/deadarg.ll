define internal i32 @f(i32 %x, i32 %y) {
; CHECK: start f 1
    ret i32 %y
}
; CHECK: end f
define internal i32 @f2(i32 %y) {
; CHECK: start f2 1
; CHECK: .entry:
    %x = call i32 @f(i32 0, i32 1)
    %ret = add i32 %x, %y
    ret i32 %ret
}
; CHECK: end f2