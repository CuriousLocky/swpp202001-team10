define i32 @g() {
    ret i32 0
}
define i32 @f() {
    %x = call i32 @g()
    ret i32 %x
}
