define i32 @main(i32 %x, i32 %y) {
; CHECK: start main 0:
  %temp1 = call i32 @func(i32 1, i32 2, i32 3)
  %temp2 = call i32 @func(i32 %temp1, i32 2, i32 3)
  %temp3 = call i32 @func(i32 %temp1, i32 %temp2, i32 3)
  %temp4 = call i32 @func(i32 %temp1, i32 %temp2, i32 %temp3)
  %temp5 = call i32 @func(i32 %temp2, i32 %temp3, i32 %temp4)
  %temp6 = call i32 @func(i32 %temp1, i32 %temp2, i32 %temp5)
  %temp7 = call i32 @func(i32 %temp2, i32 %temp4, i32 %temp6)
  %temp8 = call i32 @func(i32 %temp7, i32 %temp4, i32 %temp3)
  ret i32 0
}
; CHECK: end main
declare i32 @func(i32, i32, i32)