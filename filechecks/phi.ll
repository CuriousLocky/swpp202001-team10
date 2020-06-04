define i32 @f(i32 %a, i32 %b) {
bb_entry:
  br label %bb_loop

bb_loop:
  %i = phi i32[%a, %bb_entry], [%i_new, %bb_loop_check]
  br label %bb_loop_check

bb_loop_check:
  %i_new = add i32 %i, 1
  %cond = icmp eq i32 %i_new, %b
  br i1 %cond, label %bb_exit, label %bb_loop

bb_exit:
  ret i32 0
}