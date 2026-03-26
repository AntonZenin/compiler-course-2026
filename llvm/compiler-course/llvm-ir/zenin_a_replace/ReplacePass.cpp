#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

static int getPowerOfTwo(llvm::Value *value) {
  if (auto *constInt = llvm::dyn_cast<llvm::ConstantInt>(value)) {
    int64_t val = constInt->getSExtValue();
    if (val > 0 && (val & (val - 1)) == 0) {
      return constInt->getValue().exactLogBase2();
    }
  }
  return -1;
}

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

          int shift = getPowerOfTwo(inst.getOperand(1));
          if (shift < 0) {
            continue;
          }

          llvm::IRBuilder<> builder(binOp);
          llvm::Value *shiftAmount =
              llvm::ConstantInt::get(inst.getType(), shift);
          llvm::Value *newInst = nullptr;

          if (opcode == llvm::Instruction::Mul) {
            newInst = builder.CreateShl(binOp->getOperand(0), shiftAmount);
          } else if (opcode == llvm::Instruction::SDiv) {
            newInst = builder.CreateAShr(binOp->getOperand(0), shiftAmount);
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
                  if (name == "replace-pass") {
                    FPM.addPass(ZeninReplacePass{});
                    return true;
                  }
                  return false;
                });
          }};
}
