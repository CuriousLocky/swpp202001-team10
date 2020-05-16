define i32 @main(i32 %x, i32 %y) {
; CHECK-LABEL: @main(i32 %x, i32 %y)
; CHECK-NEXT:    [[malloc_var:%.*]] = alloca i32
; CHECK-NEXT:    [[A:%.*]] = add i32 2, 5
; CHECK-NEXT:    ret i32 [[A]]
;

  %malloc_var = call i8* @malloc(i32 8)
  %a = add i32 2, 5
  call void @free(i8* %malloc_var)
  ret i32 %a
}
declare void @free(i8*)
declare i8* @malloc(i32)
