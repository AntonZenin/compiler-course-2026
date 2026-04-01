#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct ZeninReplacePass : llvm::PassInfoMixin<ZeninReplacePass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    bool changed = false;

    for (auto &block : func) {
      for (auto &inst : llvm::make_early_inc_range(block)) {
        if (auto *binOp = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
          unsigned opcode = binOp->getOpcode();
          if (opcode != llvm::Instruction::Mul &&
              opcode != llvm::Instruction::SDiv &&
              opcode != llvm::Instruction::UDiv) {
            continue;
          }

          auto *constInt =
              llvm::dyn_cast<llvm::ConstantInt>(binOp->getOperand(1));
          if (!constInt || !constInt->getValue().isPowerOf2())
            continue;
          int shift = constInt->getValue().exactLogBase2();

          llvm::IRBuilder<> builder(binOp);
          llvm::Value *shiftAmount =
              llvm::ConstantInt::get(inst.getType(), shift);
          llvm::Value *newInst = nullptr;

          if (opcode == llvm::Instruction::Mul) {
            newInst = builder.CreateShl(binOp->getOperand(0), shiftAmount);
          } else if (opcode == llvm::Instruction::SDiv) {
            llvm::Value *lhs = binOp->getOperand(0);
            llvm::Type *type = binOp->getType();
            unsigned bitWidth = type->getIntegerBitWidth();

            llvm::Value *signShift = llvm::ConstantInt::get(type, bitWidth - 1);
            llvm::Value *sign = builder.CreateAShr(lhs, signShift);

            llvm::Value *maskVal = llvm::ConstantInt::get(
                type, llvm::APInt::getLowBitsSet(bitWidth, shift));
            llvm::Value *mask = builder.CreateAnd(sign, maskVal);

            llvm::Value *corrected = builder.CreateAdd(lhs, mask);
            newInst = builder.CreateAShr(corrected, shiftAmount);
          } else {
            newInst = builder.CreateLShr(binOp->getOperand(0), shiftAmount);
          }

          binOp->replaceAllUsesWith(newInst);
          binOp->eraseFromParent();
          changed = true;
        }
      }
    }
    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ReplacePass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "Replace-pass") {
                    FPM.addPass(ZeninReplacePass{});
                    return true;
                  }
                  return false;
                });
          }};
}