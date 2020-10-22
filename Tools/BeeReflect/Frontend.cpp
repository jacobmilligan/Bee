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
#include <clang/Lex/HeaderSearch.h>



namespace bee {
namespace reflect {



BeeReflectFrontendActionFactory::BeeReflectFrontendActionFactory()
    : allocator(megabytes(8), megabytes(8)),
      storage(system_allocator())
{}

std::unique_ptr<clang::FrontendAction> BeeReflectFrontendActionFactory::create()
{
    return std::make_unique<BeeReflectFrontendAction>(&storage, &allocator);
}


BeeReflectFrontendAction::BeeReflectFrontendAction(TypeMap* storage, ReflectionAllocator* allocator)
    : matcher_(storage, allocator)
{
    /*
     * Match any record with an __annotate__ attribute and bind it to "id" - don't match nodes that have parents that
     * are records as we're going to recursively reflect those manually when its parent is matched
     */
    auto decl_matcher = clang::ast_matchers::cxxRecordDecl(
        clang::ast_matchers::unless(clang::ast_matchers::hasAncestor(clang::ast_matchers::recordDecl())),
        clang::ast_matchers::unless(clang::ast_matchers::classTemplateSpecializationDecl()),
        clang::ast_matchers::hasAttr(clang::attr::Annotate)
    ).bind("id");
    auto enum_matcher = clang::ast_matchers::enumDecl(
        clang::ast_matchers::unless(clang::ast_matchers::hasAncestor(clang::ast_matchers::recordDecl())),
        clang::ast_matchers::hasAttr(clang::attr::Annotate)
    ).bind("id");
    auto function_matcher = clang::ast_matchers::functionDecl(
        clang::ast_matchers::unless(clang::ast_matchers::hasAncestor(clang::ast_matchers::recordDecl())),
        clang::ast_matchers::hasAttr(clang::attr::Annotate)
    ).bind("id"); // ignore method decls as we're going to reflect those as child nodes when a record is matched

    finder_.addMatcher(decl_matcher, &matcher_);
    finder_.addMatcher(enum_matcher, &matcher_);
    finder_.addMatcher(function_matcher, &matcher_);
}


std::unique_ptr<clang::ASTConsumer> BeeReflectFrontendAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef InFile)
{
    CI.getHeaderSearchOpts().UseBuiltinIncludes = false;
    CI.getHeaderSearchOpts().UseStandardSystemIncludes = false;
    CI.getHeaderSearchOpts().UseStandardCXXIncludes = false;
//    CI.getPreprocessor().SetSuppressIncludeNotFoundError(true);

    for (const auto& path : CI.getHeaderSearchOpts().UserEntries)
    {
        if (path.Group == clang::frontend::IncludeDirGroup::Quoted || path.Group == clang::frontend::IncludeDirGroup::Angled)
        {
            matcher_.type_map->include_dirs.push_back(Path(path.Path.c_str()).normalize());
        }
    }
    return finder_.newASTConsumer();
}

bool BeeReflectFrontendAction::BeginInvocation(clang::CompilerInstance& CI)
{
    auto& preprocessor_opts = CI.getInvocation().getPreprocessorOpts();
    preprocessor_opts.addMacroDef("BEE_COMPILE_REFLECTION");

    CI.getInvocation().getFrontendOpts().SkipFunctionBodies = true;
    matcher_.diagnostics.init(&CI.getDiagnostics());
    return true;
}


} // namespace reflect
} // namespace bee