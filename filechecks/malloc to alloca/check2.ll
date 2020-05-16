define i32 @main(i32 %x, i32 %y) {
; CHECK-LABEL: @main(i32 %x, i32 %y)
; CHECK-NEXT:    [[malloc_var:%.*]] = call i8* @malloc(i32 4)
; CHECK-NEXT:    [[A:%.*]] = add i32 2, 5
; CHECK-NEXT:    ret i32 [[A]]
;

  %malloc_var = call i8* @malloc(i32 4)
  %a = add i32 2, 5
  ret i32 %a
}
declare void @free(i8*)
declare i8* @malloc(i32)
