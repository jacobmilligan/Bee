/*
 *  Frontend.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "RecordFinder.hpp"

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>


namespace bee {



class ClangReflectFrontendAction final : public clang::ASTFrontendAction
{
public:
    explicit ClangReflectFrontendAction(DynamicArray<Type*>* types, ReflectionAllocator* allocator);
protected:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef InFile) override;

    bool BeginInvocation(clang::CompilerInstance& CI) override;
private:
    clang::ast_matchers::MatchFinder    finder_;
    RecordFinder                        record_finder_;
};


struct ClangReflectFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
    ReflectionAllocator   allocator;
    DynamicArray<Type*>   types;

    ClangReflectFrontendActionFactory();

    std::unique_ptr<clang::FrontendAction> create() override;
};


} // namespace bee