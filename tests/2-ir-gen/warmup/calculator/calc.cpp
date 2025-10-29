extern "C" {
#include "syntax_tree.h"
extern syntax_tree *parse(const char *);
}
#include "calc_ast.hpp"
#include "calc_builder.hpp"

#include <cstdio>
#include <fstream>
using namespace std::literals::string_literals;

int main(int argc, char *argv[]) {
    syntax_tree *tree = NULL;
    const char *input = NULL;

    if (argc >= 3) {
        printf("usage: %s\n", argv[0]);
        printf("usage: %s <cminus_file>\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        input = argv[1];
    } else {
        printf("Input an arithmatic expression (press Ctrl+D in a new line after you finish the expression):\n");
    }
    // 建立syntax_tree, gt->root存放syntax_tree根结点
    tree = parse(input);
    // 有参构造，使用syntax_tree构造AST
    CalcAST ast(tree);
    // 创建一个builder对象
    CalcBuilder builder;
    // 使用AST创建moudle
    auto module = builder.build(ast);
    // 以下都是打印指令的部分
    auto IR = module->print();

    std::ofstream output_stream;
    auto output_file = "result.ll";
    output_stream.open(output_file, std::ios::out);
    output_stream << "; ModuleID = 'calculator'\n";
    output_stream << IR;
    output_stream.close();
    auto command_string = "clang -O0 -w "s + "result.ll -o result -L. -lcminus_io";
    auto ret = std::system(command_string.c_str());
    if (ret) {
        printf("something went wrong!\n");
    } else {
        printf("result and result.ll have been generated.\n");
    }
    return ret;
}
