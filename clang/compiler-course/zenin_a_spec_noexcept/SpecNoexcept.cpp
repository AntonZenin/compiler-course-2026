#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class ThrowFinder final : public clang::RecursiveASTVisitor<ThrowFinder> {
public:
  bool VisitCXXThrowExpr(clang::CXXThrowExpr *) {
    m_hasThrow = true;
    
    return false;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (auto *callee = call->getDirectCallee()) {
      auto specType = callee->getExceptionSpecType();
      if (specType != clang::EST_BasicNoexcept &&
          specType != clang::EST_NoexceptTrue) {
        m_hasThrow = true;
        return false;
      }
    }
    return true;
  }

  bool hasThrow() const { return m_hasThrow; }

private:
  bool m_hasThrow = false;
};

class SpecNoexceptVisitor final
    : public clang::RecursiveASTVisitor<SpecNoexceptVisitor> {
public:
  explicit SpecNoexceptVisitor(clang::ASTContext *context,
                               clang::Rewriter &rewriter)
      : m_rewriter(rewriter) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func) {

    llvm::errs() << "Visiting: " << func->getNameAsString() << "\n";

    if (!func->hasBody() ||
        func->getExceptionSpecType() == clang::EST_BasicNoexcept) {
      return true;
    }

    ThrowFinder finder;
    finder.TraverseStmt(func->getBody());

    if (!finder.hasThrow()) {
      clang::SourceLocation loc = func->getFunctionTypeLoc().getRParenLoc();
      m_rewriter.InsertTextAfter(loc.getLocWithOffset(1), " noexcept");
      func->dump();
    }
    return true;
  }

private:
  clang::Rewriter &m_rewriter;
};

class SpecNoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit SpecNoexceptConsumer(clang::ASTContext *context,
                                clang::Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  SpecNoexceptVisitor m_visitor;
};

class SpecNoexceptAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<SpecNoexceptConsumer>(&ci.getASTContext(),
                                                  m_rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

private:
  clang::Rewriter m_rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<SpecNoexceptAction>
    X("spec_noexcept", "Add noexcept specifier to functions that don't throw");
