define i32 @main(i32 %x, i32 %y) {
; CHECK-LABEL: @main(i32 %x, i32 %y)
; CHECK-NEXT:    [[A:%.*]] = add i32 2, 5
; CHECK-NEXT:    ret i32 [[A]]
;

  %a = add i32 2, 5
  ret i32 %a
}
declare void @free(i8*)
declare i8* @malloc(i32)
