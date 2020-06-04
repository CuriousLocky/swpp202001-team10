; ModuleID = 'filechecks/regEmit.ll'
source_filename = "filechecks/regEmit.ll"

define i32 @main(i32 %__arg__0, i32 %__arg__1) {
entry:
  call void @__spSub(i64 12)
  %r1_temp1 = call i32 @func(i32 1, i32 2, i32 3)
  %r2_temp2 = call i32 @func(i32 %r1_temp1, i32 2, i32 3)
  %r3_temp3 = call i32 @func(i32 %r1_temp1, i32 %r2_temp2, i32 3)
  %r4_temp4 = call i32 @func(i32 %r1_temp1, i32 %r2_temp2, i32 %r3_temp3)
  %temp_p_r1_temp1 = call i8* @__spOffset(i64 0)
  %temp_p_r1_temp1_cast = bitcast i8* %temp_p_r1_temp1 to i32*
  store i32 %r1_temp1, i32* %temp_p_r1_temp1_cast
  %r1_temp5 = call i32 @func(i32 %r2_temp2, i32 %r3_temp3, i32 %r4_temp4)
  %temp_p_r1_temp5 = call i8* @__spOffset(i64 4)
  %temp_p_r1_temp5_cast = bitcast i8* %temp_p_r1_temp5 to i32*
  store i32 %r1_temp5, i32* %temp_p_r1_temp5_cast
  %temp_p_r1_temp11 = call i8* @__spOffset(i64 0)
  %temp_p_r1_temp11_cast = bitcast i8* %temp_p_r1_temp11 to i32*
  %r1_r1_temp1 = load i32, i32* %temp_p_r1_temp11_cast
  %temp_p_r3_temp3 = call i8* @__spOffset(i64 8)
  %temp_p_r3_temp3_cast = bitcast i8* %temp_p_r3_temp3 to i32*
  store i32 %r3_temp3, i32* %temp_p_r3_temp3_cast
  %temp_p_r1_temp52 = call i8* @__spOffset(i64 4)
  %temp_p_r1_temp52_cast = bitcast i8* %temp_p_r1_temp52 to i32*
  %r3_r1_temp5 = load i32, i32* %temp_p_r1_temp52_cast
  %temp_p_r4_temp4 = call i8* @__spOffset(i64 4)
  %temp_p_r4_temp4_cast = bitcast i8* %temp_p_r4_temp4 to i32*
  store i32 %r4_temp4, i32* %temp_p_r4_temp4_cast
  %r4_temp6 = call i32 @func(i32 %r1_r1_temp1, i32 %r2_temp2, i32 %r3_r1_temp5)
  %temp_p_r4_temp6 = call i8* @__spOffset(i64 0)
  %temp_p_r4_temp6_cast = bitcast i8* %temp_p_r4_temp6 to i32*
  store i32 %r4_temp6, i32* %temp_p_r4_temp6_cast
  %temp_p_r4_temp43 = call i8* @__spOffset(i64 4)
  %temp_p_r4_temp43_cast = bitcast i8* %temp_p_r4_temp43 to i32*
  %r4_r4_temp4 = load i32, i32* %temp_p_r4_temp43_cast
  %temp_p_r3_temp34 = call i8* @__spOffset(i64 8)
  %temp_p_r3_temp34_cast = bitcast i8* %temp_p_r3_temp34 to i32*
  %r3_r3_temp3 = load i32, i32* %temp_p_r3_temp34_cast
  %temp_p_r4_temp65 = call i8* @__spOffset(i64 0)
  %temp_p_r4_temp65_cast = bitcast i8* %temp_p_r4_temp65 to i32*
  %r1_r4_temp6 = load i32, i32* %temp_p_r4_temp65_cast
  %temp_p_r3_r3_temp3 = call i8* @__spOffset(i64 8)
  %temp_p_r3_r3_temp3_cast = bitcast i8* %temp_p_r3_r3_temp3 to i32*
  store i32 %r3_r3_temp3, i32* %temp_p_r3_r3_temp3_cast
  %r3_temp7 = call i32 @func(i32 %r2_temp2, i32 %r4_r4_temp4, i32 %r1_r4_temp6)
  %temp_p_r3_temp7 = call i8* @__spOffset(i64 0)
  %temp_p_r3_temp7_cast = bitcast i8* %temp_p_r3_temp7 to i32*
  store i32 %r3_temp7, i32* %temp_p_r3_temp7_cast
  %temp_p_r3_r3_temp36 = call i8* @__spOffset(i64 8)
  %temp_p_r3_r3_temp36_cast = bitcast i8* %temp_p_r3_r3_temp36 to i32*
  %r3_r3_r3_temp3 = load i32, i32* %temp_p_r3_r3_temp36_cast
  %temp_p_r3_temp77 = call i8* @__spOffset(i64 0)
  %temp_p_r3_temp77_cast = bitcast i8* %temp_p_r3_temp77 to i32*
  %r1_r3_temp7 = load i32, i32* %temp_p_r3_temp77_cast
  %r2_temp8 = call i32 @func(i32 %r1_r3_temp7, i32 %r4_r4_temp4, i32 %r3_r3_r3_temp3)
  ret i32 0
}

declare i32 @func(i32, i32, i32)

declare void @__resetHeap(void)

declare void @__resetStack(void)

declare i8* @__spOffset(i64)

declare void @__spSub(i64)
