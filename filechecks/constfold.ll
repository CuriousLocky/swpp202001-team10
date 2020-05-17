define i32 @f() {
; CHECK: start f 0
; CHECK: .entry
; CHECK: ret 8
; CHECK: end f
    %y = add i32 1, 1
    %z = add i32 %y, 2
    %w = mul i32 %z, 2
    ret i32 %w
}