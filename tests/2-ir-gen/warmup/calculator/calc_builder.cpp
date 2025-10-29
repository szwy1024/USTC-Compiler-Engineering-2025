#include "calc_builder.hpp"
#include <memory>
std::unique_ptr<Module> CalcBuilder::build(CalcAST &ast) {
    // 创建module
    module = std::unique_ptr<Module>(new Module());
    // 创建IRBuilder
    builder = std::make_unique<IRBuilder>(nullptr, module.get());
    // 获取基本数据类型
    auto TyVoid = module->get_void_type();
    TyInt32 = module->get_int32_type();

    // 创建out_put函数的参数列表
    std::vector<Type *> output_params;
    output_params.push_back(TyInt32);
    // 创建函数的参数类型和返回值类型
    auto output_type = FunctionType::get(TyVoid, output_params);
    // 创建out_put函数
    auto output_fun = Function::create(output_type, "output", module.get());
    // 创建main函数
    auto main =
        Function::create(FunctionType::get(TyInt32, {}), "main", module.get());
    // 创建main函数中的基本块
    auto bb = BasicBlock::create(module.get(), "entry", main);
    // 将IRBuilder插入指令位置设置为 bb 尾部
    builder->set_insert_point(bb);
    // 调用AST的run_visitor函数，将当前builder对象作为参数传递
    // void CalcAST::run_visitor(CalcASTVisitor &visitor) { root->accept(visitor); }
    // run_visitor函数内部会调用accept函数，接受当前builder对象作为参数
    // void CalcASTInput::accept(CalcASTVisitor &visitor) { expression->accept(visitor); }
    // void CalcASTExpression::accept(CalcASTVisitor &visitor) { visitor.visit(*this); }
    // accept函数内部最终会调用Visitor的visit函数，会传入当前的AST结点 
    ast.run_visitor(*this);
    builder->create_call(output_fun, {val});
    builder->create_ret(ConstantInt::get(0, module.get()));
    return std::move(module);
}
// 下面这个函数是不是永远不会被调用？
void CalcBuilder::visit(CalcASTInput &node) { node.expression->accept(*this); }
void CalcBuilder::visit(CalcASTExpression &node) {
    if (node.expression == nullptr) {
        // 如果使用产生式 expression : trem，使用term调用accept函数
        node.term->accept(*this);
    } else {
        // 调用左子树的accept函数
        node.expression->accept(*this);
        auto l_val = val;
        node.term->accept(*this);
        auto r_val = val;
        switch (node.op) {
        case OP_PLUS:
            val = builder->create_iadd(l_val, r_val);
            break;
        case OP_MINUS:
            val = builder->create_isub(l_val, r_val);
            break;
        }
    }
}

void CalcBuilder::visit(CalcASTTerm &node) {
    if (node.term == nullptr) {
        node.factor->accept(*this);
    } else {
        node.term->accept(*this);
        auto l_val = val;
        node.factor->accept(*this);
        auto r_val = val;
        switch (node.op) {
        case OP_MUL:
            val = builder->create_imul(l_val, r_val);
            break;
        case OP_DIV:
            val = builder->create_isdiv(l_val, r_val);
            break;
        }
    }
}

void CalcBuilder::visit(CalcASTNum &node) {
    // 用Builder中的val来保存node.val
    val = ConstantInt::get(node.val, module.get());
}
