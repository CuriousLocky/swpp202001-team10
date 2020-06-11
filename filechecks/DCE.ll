define i32 @main(i32 %x, i32 %y){
; CHECK: start main 2:
; CHECK: .entry:
; CHECK: ret arg1
; CHECK: end main
    %z = add i32 %x, %y
    ret i32 %x
}
