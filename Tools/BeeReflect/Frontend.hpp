/*
 *  Frontend.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "ASTMatcher.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    BEE_DISABLE_WARNING_MSVC(4996)
    #include <clang/Frontend/FrontendAction.h>
    #include <clang/Tooling/Tooling.h>
BEE_POP_WARNING


namespace bee {
namespace reflect {


class BeeReflectFrontendAction final : public clang::ASTFrontendAction
{
public:
    explicit BeeReflectFrontendAction(TypeMap* storage, ReflectionAllocator* allocator);

    bool PrepareToExecuteAction(clang::CompilerInstance& CI) override;
protected:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef InFile) override;

    bool BeginInvocation(clang::CompilerInstance& CI) override;
private:
    clang::ast_matchers::MatchFinder    finder_;
    ASTMatcher                          matcher_;
};


struct BeeReflectFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    ReflectionAllocator     allocator;
    TypeMap                 storage;

    BeeReflectFrontendActionFactory();

    std::unique_ptr<clang::FrontendAction> create() override;
};


} // namespace reflect
} // namespace bee