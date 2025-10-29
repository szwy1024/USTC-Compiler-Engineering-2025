#include "calc_ast.hpp"

#include <cstring>
#include <iostream>
#include <stack>
#define _AST_NODE_ERROR_                                                       \
    std::cerr << "Abort due to node cast error."                               \
                 "Contact with TAs to solve your problem."                     \
              << std::endl;                                                    \
    std::abort();
#define _STR_EQ(a, b) (strcmp((a), (b)) == 0)

void CalcAST::run_visitor(CalcASTVisitor &visitor) { root->accept(visitor); }

CalcAST::CalcAST(syntax_tree *s) {
    if (s == nullptr) {
        std::cerr << "empty input tree!" << std::endl;
        std::abort();
    }
    auto node = transform_node_iter(s->root);
    del_syntax_tree(s);
    // 将返回的node类型下转为CalcASTInput
    // 完成使用syntax-tree构造AST的过程，root保存指向CalcASTInput的指针
    root = std::shared_ptr<CalcASTInput>(static_cast<CalcASTInput *>(node));
}

CalcASTNode *CalcAST::transform_node_iter(syntax_tree_node *n) {
    // 根据syntax_tree中的name字段来判断类型（这里是不是也可以使用访问者模式？）
    if (_STR_EQ(n->name, "input")) {
        // 建立expression子树
        auto node = new CalcASTInput();
        auto expr_node = static_cast<CalcASTExpression *>(
            transform_node_iter(n->children[0]));
        node->expression = std::shared_ptr<CalcASTExpression>(expr_node);
        return node;
    } else if (_STR_EQ(n->name, "expression")) {
        // 根据syntax_tree中孩子结点的个数来判断使用哪个产生式
        auto node = new CalcASTExpression();
        // 如果使用产生式 expression : expression addop term
        if (n->children_num == 3) {
            // 递归建立expression左子树
            auto add_expr_node = static_cast<CalcASTExpression *>(
                transform_node_iter(n->children[0]));
            node->expression =
                std::shared_ptr<CalcASTExpression>(add_expr_node);
            // 获取运算符
            auto op_name = n->children[1]->children[0]->name;
            if (_STR_EQ(op_name, "+"))
                node->op = OP_PLUS;
            else if (_STR_EQ(op_name, "-"))
                node->op = OP_MINUS;
            // 递归建立term右子树
            auto term_node =
                static_cast<CalcASTTerm *>(transform_node_iter(n->children[2]));
            node->term = std::shared_ptr<CalcASTTerm>(term_node);
        } else {
            // 如果使用产生式 expression :term
            // 建立term子树
            auto term_node =
                static_cast<CalcASTTerm *>(transform_node_iter(n->children[0]));
            node->term = std::shared_ptr<CalcASTTerm>(term_node);
        }
        return node;
    } else if (_STR_EQ(n->name, "term")) {
        auto node = new CalcASTTerm();
        // 同expression产生式，也有两种情况
        if (n->children_num == 3) {
            // 递归建立term左子树
            auto term_node =
                static_cast<CalcASTTerm *>(transform_node_iter(n->children[0]));
            node->term = std::shared_ptr<CalcASTTerm>(term_node);

            auto op_name = n->children[1]->children[0]->name;
            if (_STR_EQ(op_name, "*"))
                node->op = OP_MUL;
            else if (_STR_EQ(op_name, "/"))
                node->op = OP_DIV;

            auto factor_node = static_cast<CalcASTFactor *>(
                transform_node_iter(n->children[2]));
            node->factor = std::shared_ptr<CalcASTFactor>(factor_node);
        } else {
            // 建立factor子树
            auto factor_node = static_cast<CalcASTFactor *>(
                transform_node_iter(n->children[0]));
            node->factor = std::shared_ptr<CalcASTFactor>(factor_node);
        }
        return node;
    } else if (_STR_EQ(n->name, "factor")) {
        if (n->children_num == 3) {
            // 如果是括号括起来的表达式，最后肯定被规约成了expression
            return transform_node_iter(n->children[1]);
        } else {
            // n->children[0]是num结点，n->children[0]->children[0]是叶结点，其成员name记录数字的具体值
            auto num_node = new CalcASTNum();
            num_node->val = std::stoi(n->children[0]->children[0]->name);
            return num_node;
        }
    } else {
        std::cerr << "[calc_ast]: transform failure!" << std::endl;
        std::abort();
    }
}

// 每个抽象语法树的结点都要重写accep方法
// 
void CalcASTNum::accept(CalcASTVisitor &visitor) { visitor.visit(*this); }
void CalcASTTerm::accept(CalcASTVisitor &visitor) { visitor.visit(*this); }
void CalcASTExpression::accept(CalcASTVisitor &visitor) {
    visitor.visit(*this);
}

void CalcASTInput::accept(CalcASTVisitor &visitor) {
    // 这里代码好像有点问题，应该先visit自身
    // void CalcBuilder::visit(CalcASTInput &node) { node.expression->accept(*this); }
    // 然后在CalcBuilder::visit中调用node.expression->accept(*this)，接受visitor对象
    expression->accept(visitor);
}

void CalcASTFactor::accept(CalcASTVisitor &visitor) {
    // 动态类型转换，转换失败返回nullptr
    // 这里也同理
    // 首先visitor.visit(*this);
    // 然后在 void CalcBuilder::visit(CalcASTFactor &node) 中 对node进行动态类型转换
    auto expr = dynamic_cast<CalcASTExpression *>(this);
    if (expr) {
        expr->accept(visitor);
        return;
    }

    auto num = dynamic_cast<CalcASTNum *>(this);
    if (num) {
        num->accept(visitor);
        return;
    }

    _AST_NODE_ERROR_
}
