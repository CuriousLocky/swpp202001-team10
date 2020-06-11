define i32 @main(i32 %x) {
; CHECK: start main 1:
; CHECK: ret arg1
; CHECK: end main
    %p = alloca i32
    br label %end
end:
    store i32 %x, i32* %p
    %q = load i32, i32* %p
    ret i32 %q
}
