
#include "Module.hpp"
#include "PassManager.hpp"
#include "ast.hpp"
#include "cminusf_builder.hpp"
#include "PassManager.hpp"
#include "DeadCode.hpp"
#include "Mem2Reg.hpp"
#include "ConstPropagation.hpp"
#include "FunctionInline.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using std::string;
using std::operator""s;

struct Config {
    string exe_name; // compiler exe name
    std::filesystem::path input_file;
    std::filesystem::path output_file;

    bool emitast{false};
    bool emitllvm{false};
    // optization config
    bool const_prop{false};
    bool dce{false};
    bool func_inline{false};

    Config(int argc, char **argv) : argc(argc), argv(argv) {
        // 解析命令行参数，并进行设置
        parse_cmd_line();
        // 进行配置检查
        check();
    }

  private:
    int argc{-1};
    char **argv{nullptr};

    void parse_cmd_line();
    void check();
    // print helper infomation and exit
    void print_help() const;
    void print_err(const string &msg) const;
};

int main(int argc, char **argv) {
    // 构造配置对象，解析命令行参数
    Config config(argc, argv);

    // 解析语法分析器输入，进行flatten操作，生成抽象语法树AST
    auto syntax_tree = parse(config.input_file.c_str());
    auto ast = AST(syntax_tree);

    // 如果设置了emitast选项，则输出AST并退出，不输出LLVM IR
    if (config.emitast) { // if emit ast (lab1), print ast and return
        ASTPrinter printer;
        ast.run_visitor(printer);
    } else {
        // 使用访问者设计模式，将AST转换为LLVM IR
        std::unique_ptr<Module> m;
        CminusfBuilder builder;
        ast.run_visitor(builder);
        m = builder.getModule();

        // 将module传入PassManager，进行一系列的优化
        PassManager PM(m.get());
        // optimization 
        if(config.dce) {
            PM.add_pass<Mem2Reg>();
            PM.add_pass<DeadCode>();
        }

        if(config.func_inline) {
            PM.add_pass<FunctionInline>();
            PM.add_pass<DeadCode>();
        }

        if(config.const_prop) {
            PM.add_pass<Mem2Reg>();
            PM.add_pass<DeadCode>();
            PM.add_pass<ConstPropagation>();
            PM.add_pass<DeadCode>();
        }
        PM.run();

        std::ofstream output_stream(config.output_file);
        // 如果设置了emitllvm选项，则输出LLVM IR
        if (config.emitllvm) {
            auto abs_path = std::filesystem::canonical(config.input_file);
            output_stream << "; ModuleID = 'cminus'\n";
            output_stream << "source_filename = " << abs_path << "\n\n";
            output_stream << m->print();
        } 
    }

    return 0;
}

void Config::parse_cmd_line() {
    // 获取第一个命令行参数作为程序名
    exe_name = argv[0];
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-h"s || argv[i] == "--help"s) {
            // 打印帮助信息并退出
            print_help();
        } else if (argv[i] == "-o"s) {
            // 获取输出文件名
            if (output_file.empty() && i + 1 < argc) {
                output_file = argv[i + 1];
                i += 1;
            } else {
                print_err("bad output file");
            }
        } else if (argv[i] == "-emit-ast"s) {
            // 打印抽象语法树
            emitast = true;
        } else if (argv[i] == "-emit-llvm"s) {
            // 输出llvm ir
            emitllvm = true;
        } else if (argv[i] == "-dce"s) {
            // 死代码消除
            dce = true;
        } else if (argv[i] == "-const-prop"s) {
            // 常量传播
            const_prop = true;
        } else if (argv[i] == "-func-inline"s) {
            // 函数内联
            func_inline = true;
        } else {
            // 如果前面都没匹配上，作为输入文件名
            if (input_file.empty()) {
                input_file = argv[i];
            } else {
                string err =
                    "unrecognized command-line option \'"s + argv[i] + "\'"s;
                print_err(err);
            }
        }
    }
}

void Config::check() {
    if (input_file.empty()) {
        print_err("no input file");
    }
    if (input_file.extension() != ".cminus") {
        print_err("file format not recognized");
    }
    if (const_prop && not dce) {
        // 常量传播需要先进行死代码消除
        print_err("const-prop pass need dce pass");
    }
    if (func_inline && not dce) {
        // 函数内联需要先进行死代码消除
        print_err("function inline pass need dce pass");
    }
    if (output_file.empty()) {
        output_file = input_file.stem();
        if (emitllvm) {
            output_file.replace_extension(".ll");
        }
    }
}

void Config::print_help() const {
    std::cout << "Usage: " << exe_name
              << " [-h|--help] [-o <target-file>] [-emit-llvm] [-S] [-dump-json]"
                 "[-const-prop] [-dce]"
                 "<input-file>"
              << std::endl;
    exit(0);
}

void Config::print_err(const string &msg) const {
    std::cout << exe_name << ": " << msg << std::endl;
    exit(-1);
}