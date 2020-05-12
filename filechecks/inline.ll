define i32 @g() {
; CHECK: start g 0
; CHECK: .entry
; CHECK-NEXT: ret 0
    ret i32 0
}
define i32 @f() {
; CHECK: start f 0
; CHECK: .entry
; CHECK: ret 0
    %x = call i32 @g()
    ret i32 %x
}
