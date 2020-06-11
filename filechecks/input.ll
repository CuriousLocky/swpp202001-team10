define i32 @main(i32 %x, i32 %y) { 
    ; CHECK: start main 2: 
    ; CHECK: .entry: 
    ; CHECK: .BB_true: 
    ; CHECK:  br .BB_end 
    ; CHECK: ret arg1 
    ; CHECK: end main
      
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
  ;shl
  %reg0 = icmp eq i32 %y, %shl0
  %reg1 = icmp eq i32 %y, %shl1
  %reg2 = icmp eq i32 %y, %shl2
  ;ashr
  %reg3 = icmp eq i32 %y, %ashr0
  %reg4 = icmp eq i32 %y, %ashr1
  %reg5 = icmp eq i32 %y, %ashr2
  ;add
  %reg6 = icmp eq i32 %y, %add0
  %reg7 = icmp eq i32 %y, %add1
  %reg8 = icmp eq i32 %y, %add2
  ;sub
  %reg9 = icmp eq i32 %y, %sub0
  %reg10 = icmp eq i32 %y, %sub1
  %reg11 = icmp eq i32 %y, %sub2
  ;sdiv
  %reg12 = icmp eq i32 %y, %sdiv0
  %reg13 = icmp eq i32 %y, %sdiv1
  %reg14 = icmp eq i32 %y, %sdiv2
  ;udiv
  %reg15 = icmp eq i32 %y, %udiv0
  %reg16 = icmp eq i32 %y, %udiv1
  %reg17 = icmp eq i32 %y, %udiv2
  ;srem
  %reg18 = icmp eq i32 %y, %srem0
  %reg19 = icmp eq i32 %y, %srem1
  %reg20 = icmp eq i32 %y, %srem2 
  ;urem
  %reg21 = icmp eq i32 %y, %urem0
  %reg22 = icmp eq i32 %y, %urem1
  %reg23 = icmp eq i32 %y, %urem2
  ;or
  %reg24 = icmp eq i32 %y, %or0
  %reg25 = icmp eq i32 %y, %or1
  %reg26 = icmp eq i32 %y, %or2
  ;and
  %reg27 = icmp eq i32 %y, %and0
  %reg28 = icmp eq i32 %y, %and1
  %reg29 = icmp eq i32 %y, %and2
  ;xor
  %reg30 = icmp eq i32 %y, %xor0
  br label %BB_end
  
BB_end:
  ret i32 %x
}