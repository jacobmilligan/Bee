/*
 *  Frontend.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Frontend.hpp"

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Hash.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PreprocessorOptions.h>



namespace bee {


ClangReflectFrontendActionFactory::ClangReflectFrontendActionFactory()
    : allocator(mebibytes(4), mebibytes(4), mebibytes(4)),
      storage(system_allocator())
{}

std::unique_ptr<clang::FrontendAction> ClangReflectFrontendActionFactory::create()
{
    return std::make_unique<ClangReflectFrontendAction>(&storage, &allocator);
}


ClangReflectFrontendAction::ClangReflectFrontendAction(TypeStorage* storage, ReflectionAllocator* allocator)
    : record_finder_(storage, allocator)
{
    // Match any record with an __annotate__ attribute and bind it to "id"
    auto decl_matcher = clang::ast_matchers::cxxRecordDecl(clang::ast_matchers::recordDecl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));
    auto field_matcher = clang::ast_matchers::fieldDecl(clang::ast_matchers::decl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));
    auto function_matcher = clang::ast_matchers::functionDecl(clang::ast_matchers::decl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));

    finder_.addMatcher(decl_matcher, &record_finder_);
    finder_.addMatcher(field_matcher, &record_finder_);
    finder_.addMatcher(function_matcher, &record_finder_);
}

std::unique_ptr<clang::ASTConsumer> ClangReflectFrontendAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef InFile)
{
    return finder_.newASTConsumer();
}

bool ClangReflectFrontendAction::BeginInvocation(clang::CompilerInstance& CI)
{
    CI.getInvocation().getPreprocessorOpts().addMacroDef("BEE_COMPILE_REFLECTION");
    return true;
}



} // namespace bee