#include "X86.h"
#include "X86InstrBuilder.h" 
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h" 

using namespace llvm;

namespace {
class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private: 
  bool readsFromMemory(const MachineInstr &MI) {
    return MI.mayLoad() && !MI.isCall();
  }

  Register getBaseReg(const MachineInstr &MI) {
    const MCInstrDesc &desc = MI.getDesc();
    int memOpStart = X86II::getMemoryOperandNo(desc.TSFlags);
    if (memOpStart < 0) {
      return 0;
    }
    memOpStart += desc.getNumDefs();

    const MachineOperand &baseOp = MI.getOperand(memOpStart);
    if (!baseOp.isReg() || baseOp.getReg() == 0) {
      return 0;
    }
    return baseOp.getReg();

  }

  MachineBasicBlock *createNullHandlerBlock(MachineFunction &MF, const X86InstrInfo *TII) {
    MachineBasicBlock *nullBlock = MF.CreateMachineBasicBlock();
    MF.push_back(nullBlock);
    BuildMI(nullBlock, llvm::DebugLoc(), TII->get(X86::TRAP));

    return nullBlock;
  }
};

char NullCheckPass::ID = 0;

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &subtarget = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *TII = subtarget.getInstrInfo();
  bool changed = false;

  std::vector<MachineInstr*> instructionsToCheck;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (readsFromMemory(MI) && getBaseReg(MI) != 0) {
        instructionsToCheck.push_back(&MI);
      }
    }
  }

  for (auto *MI : instructionsToCheck) {
    MachineBasicBlock *currMBB = MI->getParent();
    Register baseReg = getBaseReg(*MI);
    
    if (baseReg == 0)
      continue;
    
    
    MachineBasicBlock *nullBlock = createNullHandlerBlock(MF, TII);
    MachineBasicBlock *continueBlock = MF.CreateMachineBasicBlock();
    
    
    MF.insert(std::next(MachineFunction::iterator(currMBB)), continueBlock);

    continueBlock->splice(continueBlock->begin(), currMBB,
                          MI->getIterator(), currMBB->end());    
    
    continueBlock->transferSuccessors(currMBB);
  
    currMBB->addSuccessor(continueBlock);
    currMBB->addSuccessor(nullBlock);
    
    
    BuildMI(currMBB, DebugLoc(), TII->get(X86::TEST64rr))
        .addReg(baseReg)
        .addReg(baseReg);
    BuildMI(currMBB, DebugLoc(), TII->get(X86::JCC_1))
        .addMBB(nullBlock)
        .addImm(X86::COND_E);
    
    BuildMI(currMBB, DebugLoc(), TII->get(X86::JMP_1))
    .addMBB(continueBlock);
    
    changed = true;
  }
  
  return changed;
}
} // namespace

static RegisterPass<NullCheckPass> X("null-check", "Null pointer check pass", false,
                                   false);

