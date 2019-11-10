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
namespace reflect {



BeeReflectFrontendActionFactory::BeeReflectFrontendActionFactory()
    : allocator(mebibytes(8), mebibytes(8)),
      storage(system_allocator())
{}

std::unique_ptr<clang::FrontendAction> BeeReflectFrontendActionFactory::create()
{
    return std::make_unique<BeeReflectFrontendAction>(&storage, &allocator);
}


BeeReflectFrontendAction::BeeReflectFrontendAction(TypeStorage* storage, ReflectionAllocator* allocator)
    : matcher_(storage, allocator)
{
    // Match any record with an __annotate__ attribute and bind it to "id"
    auto decl_matcher = clang::ast_matchers::cxxRecordDecl(clang::ast_matchers::recordDecl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));
    auto enum_matcher = clang::ast_matchers::enumDecl(clang::ast_matchers::enumDecl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));
    auto field_matcher = clang::ast_matchers::fieldDecl(clang::ast_matchers::decl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));
    auto function_matcher = clang::ast_matchers::functionDecl(clang::ast_matchers::decl().bind("id"), clang::ast_matchers::hasAttr(clang::attr::Annotate));

    finder_.addMatcher(decl_matcher, &matcher_);
    finder_.addMatcher(enum_matcher, &matcher_);
    finder_.addMatcher(field_matcher, &matcher_);
    finder_.addMatcher(function_matcher, &matcher_);
}

std::unique_ptr<clang::ASTConsumer> BeeReflectFrontendAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef InFile)
{
    return finder_.newASTConsumer();
}

bool BeeReflectFrontendAction::BeginInvocation(clang::CompilerInstance& CI)
{
    CI.getInvocation().getPreprocessorOpts().addMacroDef("BEE_COMPILE_REFLECTION");
    matcher_.diagnostics.init(&CI.getDiagnostics());
    return true;
}


} // namespace reflect
} // namespace bee