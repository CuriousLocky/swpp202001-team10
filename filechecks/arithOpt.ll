define i32 @f(i32 %x, i32 %y) {
; CHECK-LABEL: @f(i32 %x, i32 %y)
; CHECK-NEXT: [[INST1:%.*]] = mul i32 [[X:%.*]], 32
; CHECK-NEXT: [[INST2:%.*]] = udiv i32 [[X]], 32
; CHECK-NEXT: [[INST3:%.*]] = mul i32 [[X]], 2
; CHECK-NEXT: [[INST4:%.*]] = mul i32 [[X]], -1
; CHECK-NEXT: [[COND:%.*]] = icmp eq i32 [[X]], [[Y:%.*]]
; CHECK-NEXT:  br i1 [[COND]], label [[BB_TRUE:%.*]], label [[BB_END:%.*]]
; CHECK:       BB_true:
; CHECK-NEXT: [[REG0:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG1:%.*]] = icmp eq i32 [[Y]], [[INST1]]
; CHECK-NEXT: [[REG2:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG3:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG4:%.*]] = icmp eq i32 [[Y]], [[INST2]]
; CHECK-NEXT: [[REG5:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG6:%.*]] = icmp eq i32 [[Y]], [[INST3]]
; CHECK-NEXT: [[REG7:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG8:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG9:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG10:%.*]] = icmp eq i32 [[Y]], [[INST4]]
; CHECK-NEXT: [[REG11:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG12:%.*]] = icmp eq i32 [[Y]], 1
; CHECK-NEXT: [[REG13:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG14:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG15:%.*]] = icmp eq i32 [[Y]], 1
; CHECK-NEXT: [[REG16:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG17:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG18:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG19:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG20:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG21:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG22:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG23:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG24:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG25:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG26:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG27:%.*]] = icmp eq i32 [[Y]], [[X]]
; CHECK-NEXT: [[REG28:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG29:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT: [[REG30:%.*]] = icmp eq i32 [[Y]], 0
; CHECK-NEXT:  br label [[BB_END]] 
; CHECK:       BB_end:
; CHECK-NEXT:    ret i32 [[X]]
;

  %shl0 = shl i32 %x, 0
  %shl1 = shl i32 %x, 5
  %shl2 = shl i32 0, %x

  %ashr0 = ashr i32 %x, 0
  %ashr1 = ashr i32 %x, 5
  %ashr2 = ashr i32 0, %x

  %add0 = add i32 %x, %x
  %add1 = add i32 0, %x
  %add2 = add i32 %x, 0

  %sub0 = sub i32 %x, %x
  %sub1 = sub i32 0, %x
  %sub2 = sub i32 %x, 0

  %sdiv0 = sdiv i32 %x, %x
  %sdiv1 = sdiv i32 0, %x
  %sdiv2 = sdiv i32 %x, 1
 
  %udiv0 = udiv i32 %x, %x
  %udiv1 = udiv i32 0, %x
  %udiv2 = udiv i32 %x, 1

  %srem0 = srem i32 %x, %x
  %srem1 = srem i32 0, %x
  %srem2 = srem i32 %x, 1

  %urem0 = urem i32 %x, %x
  %urem1 = urem i32 0, %x
  %urem2 = urem i32 %x, 1
  
  %or0 = or i32 %x, %x
  %or1 = or i32 0, %x
  %or2 = or i32 %x, 0

  %and0 = and i32 %x, %x
  %and1 = and i32 0, %x
  %and2 = and i32 %x, 0

  %xor0 = xor i32 %x, %x

  %cond = icmp eq i32 %x, %y
  br i1 %cond, label %BB_true, label %BB_end


BB_true:
  
  %reg0 = icmp eq i32 %y, %shl0
  %reg1 = icmp eq i32 %y, %shl1
  %reg2 = icmp eq i32 %y, %shl2

  %reg3 = icmp eq i32 %y, %ashr0
  %reg4 = icmp eq i32 %y, %ashr1
  %reg5 = icmp eq i32 %y, %ashr2

  %reg6 = icmp eq i32 %y, %add0
  %reg7 = icmp eq i32 %y, %add1
  %reg8 = icmp eq i32 %y, %add2

  %reg9 = icmp eq i32 %y, %sub0
  %reg10 = icmp eq i32 %y, %sub1
  %reg11 = icmp eq i32 %y, %sub2

  %reg12 = icmp eq i32 %y, %sdiv0
  %reg13 = icmp eq i32 %y, %sdiv1
  %reg14 = icmp eq i32 %y, %sdiv2

  %reg15 = icmp eq i32 %y, %udiv0
  %reg16 = icmp eq i32 %y, %udiv1
  %reg17 = icmp eq i32 %y, %udiv2

  %reg18 = icmp eq i32 %y, %srem0
  %reg19 = icmp eq i32 %y, %srem1
  %reg20 = icmp eq i32 %y, %srem2 
 
  %reg21 = icmp eq i32 %y, %urem0
  %reg22 = icmp eq i32 %y, %urem1
  %reg23 = icmp eq i32 %y, %urem2

  %reg24 = icmp eq i32 %y, %or0
  %reg25 = icmp eq i32 %y, %or1
  %reg26 = icmp eq i32 %y, %or2

  %reg27 = icmp eq i32 %y, %and0
  %reg28 = icmp eq i32 %y, %and1
  %reg29 = icmp eq i32 %y, %and2

  %reg30 = icmp eq i32 %y, %xor0

  br label %BB_end

BB_end:
  ret i32 %x
}
