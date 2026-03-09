// RUN: %clang_cc1 -load %llvmshlibdir/zenin_a_spec_noexcept_ClangAST%pluginext -plugin spec_noexcept -fsyntax-only -fcxx-exceptions %s 2>&1 | FileCheck %s

// CHECK: Visiting: emptyFunc
// CHECK-NEXT: FunctionDecl {{0x[0-9a-fA-F]+}} {{.*}} emptyFunc 'void ()'
// CHECK-NEXT: `-CompoundStmt {{0x[0-9a-fA-F]+}} <col:18, col:19>
// CHECK: Visiting: add
// CHECK-NEXT: FunctionDecl {{0x[0-9a-fA-F]+}} {{.*}} add 'int (int, int)'
// CHECK-NEXT: |-ParmVarDecl {{0x[0-9a-fA-F]+}} <col:9, col:13> col:13 used a 'int'
// CHECK-NEXT: |-ParmVarDecl {{0x[0-9a-fA-F]+}} <col:16, col:20> col:20 used b 'int'
// CHECK-NEXT: `-CompoundStmt {{0x[0-9a-fA-F]+}} <col:23, col:39>
// CHECK: Visiting: mayThrow
// CHECK: Visiting: callsThrow

void emptyFunc() {}

int add(int a, int b) { return a + b; }

void mayThrow() { throw 42; }

void callsThrow() { mayThrow(); }

