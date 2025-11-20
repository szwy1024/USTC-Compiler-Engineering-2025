#pragma once

extern "C" {
#include "syntax_tree.h"
extern syntax_tree *parse(const char *input);
}
#include <memory>
#include <vector>

enum AddOp {
    // +
    OP_PLUS,
    // -
    OP_MINUS
};

enum MulOp {
    // *
    OP_MUL,
    // /
    OP_DIV
};

class CalcAST;

struct CalcASTNode;
struct CalcASTInput;
struct CalcASTExpression;
struct CalcASTNum;
struct CalcASTTerm;
struct CalcASTFactor;

class CalcASTVisitor;

class CalcAST {
  public:
    CalcAST() = delete;
    // AST的构造函数，使用syntax_tree构造AST
    CalcAST(syntax_tree *);
    CalcAST(CalcAST &&tree) {
        root = tree.root;
        tree.root = nullptr;
    }
    CalcASTInput *get_root() { return root.get(); }
    void run_visitor(CalcASTVisitor &visitor);

  private:
    // 传入syntax_tree_node结点，返回AST结点，进行flatten操作
    CalcASTNode *transform_node_iter(syntax_tree_node *);
    // 指向AST根结点的指针
    std::shared_ptr<CalcASTInput> root = nullptr;
};

// AST结点基类
struct CalcASTNode {
    virtual void accept(CalcASTVisitor &) = 0;
    virtual ~CalcASTNode() = default;
};

struct CalcASTInput : CalcASTNode {
    virtual void accept(CalcASTVisitor &) override final;
    std::shared_ptr<CalcASTExpression> expression;
};

struct CalcASTFactor : CalcASTNode {
    virtual void accept(CalcASTVisitor &) override;
};

struct CalcASTNum : CalcASTFactor {
    virtual void accept(CalcASTVisitor &) override final;
    int val;
};

struct CalcASTExpression : CalcASTFactor {
    virtual void accept(CalcASTVisitor &) override final;
    std::shared_ptr<CalcASTExpression> expression;
    AddOp op;
    std::shared_ptr<CalcASTTerm> term;
    // 如果使用第一个产生式，expression存放指向下一个expression的指针
};

struct CalcASTTerm : CalcASTNode {
    virtual void accept(CalcASTVisitor &) override final;
    std::shared_ptr<CalcASTTerm> term;
    MulOp op;
    std::shared_ptr<CalcASTFactor> factor;
    // 由于factor只能由num或expression规约而来，所以这里省略了factor中的成员
    // 让num类和expression类都继承自factor类，term中保存指向facotr基类的指针
};

class CalcASTVisitor {
  public:
    virtual void visit(CalcASTInput &) = 0;
    virtual void visit(CalcASTNum &) = 0;
    virtual void visit(CalcASTExpression &) = 0;
    virtual void visit(CalcASTTerm &) = 0;
};
