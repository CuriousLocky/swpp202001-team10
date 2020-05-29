@variable = internal constant i32 45 
define i32* @main(i32* %x) {
; CHECK: start main 1:
; CHECK: .entry:
; CHECK: ret arg1
; CHECK: end main
    ret i32* %x
}

