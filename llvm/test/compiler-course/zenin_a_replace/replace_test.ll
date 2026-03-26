; RUN: opt -load-pass-plugin %llvmshlibdir/zenin_a_replace_LLVM_IR%pluginext\
; RUN: -passes=replace-pass -S %s | FileCheck %s

; CHECK-LABEL: @mulByPow2
; CHECK: shl
; CHECK-NOT: mul
define i32 @mulByPow2(i32 %x) {
  %result = mul i32 %x, 4
  ret i32 %result
}

; CHECK-LABEL: @divByPow2
; CHECK: ashr
; CHECK-NOT: sdiv
define i32 @divByPow2(i32 %x) {
  %result = sdiv i32 %x, 8
  ret i32 %result
}

; CHECK-LABEL: @udivByPow2
; CHECK: lshr
; CHECK-NOT: udiv
define i32 @udivByPow2(i32 %x) {
  %result = udiv i32 %x, 4
  ret i32 %result
}

; CHECK-LABEL: @mulByNonPow2
; CHECK: mul
; CHECK-NOT: shl
define i32 @mulByNonPow2(i32 %x) {
  %result = mul i32 %x, 6
  ret i32 %result
}

; CHECK-LABEL: @divByNonPow2
; CHECK: sdiv
; CHECK-NOT: ashr
define i32 @divByNonPow2(i32 %x) {
  %result = sdiv i32 %x, 6
  ret i32 %result
}