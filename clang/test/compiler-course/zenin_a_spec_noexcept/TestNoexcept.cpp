// RUN: %clang_cc1 -load %llvmshlibdir/zenin_a_spec_noexcept_ClangAST%pluginext -plugin spec_noexcept -fsyntax-only %s 2>&1 | FileCheck %s

int add(int a, int b) { return a + b; }

void mayThrow() { throw 42; }



