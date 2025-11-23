#include "ast.hpp"

#include <cstring>
#include <iostream>
#include <stack>

#define _AST_NODE_ERROR_                                                       \
  std::cerr << "Abort due to node cast error."                                 \
               "Contact with TAs to solve your problem."                       \
            << std::endl;                                                      \
  std::abort();
// NOTE: use std::string instead of strcmp to compare strings,
// 注意使用这里的接口
#define _STR_EQ(a, b) (strcmp((a), (b)) == 0)

void AST::run_visitor(ASTVisitor &visitor) { root->accept(visitor); }

AST::AST(syntax_tree *s) {
  if (s == nullptr) {
    std::cerr << "empty input tree!" << std::endl;
    std::abort();
  }
  auto node = transform_node_iter(s->root);
  del_syntax_tree(s);
  root = std::shared_ptr<ASTProgram>(static_cast<ASTProgram *>(node));
}

// 传入一个syntax_tree_node指针，根据名称进行转换，返回对应的ASTNode指针
ASTNode *AST::transform_node_iter(syntax_tree_node *n) {

  if (_STR_EQ(n->name, "program")) {
    auto node = new ASTProgram();

    std::stack<syntax_tree_node *> s; 
    auto list_ptr = n->children[0];
    // list_ptr指向declaration-list节点
    while (list_ptr->children_num == 2) {
      // 如果declaration-list有两个子节点，说明是declaration-list declaration形，将declaration入栈
      // 然后list_ptr指向declaration-list节点，继续循环
      s.push(list_ptr->children[1]);
      list_ptr = list_ptr->children[0];
    }
    // 处理最后一个declaration节点
    s.push(list_ptr->children[0]);

    while (!s.empty()) {
      // 处理栈顶的declaration节点
      auto child_node =
          static_cast<ASTDeclaration *>(transform_node_iter(s.top()));
      // 使用shared_ptr管理内存
      auto child_node_shared = std::shared_ptr<ASTDeclaration>(child_node);
      // 添加到program的declarations列表中
      node->declarations.push_back(child_node_shared);
      // 弹出栈顶的declaration节点
      s.pop();
    }
    // 返回ASTProgram节点
    return node;
  } else if (_STR_EQ(n->name, "declaration")) {
    // declaration -> var-declaration | fun-declaration
    // declaration只有一个子节点，直接递归转换该子节点
    return transform_node_iter(n->children[0]);
  } else if (_STR_EQ(n->name, "var-declaration")) {
    auto node = new ASTVarDeclaration();
    // NOTE: 思考 ASTVarDeclaration的结构，需要填充的字段有哪些
    // type
    // 为什么不会有 TYPE_VOID?
    // 答：因为变量声明不允许void类型
    if (_STR_EQ(n->children[0]->children[0]->name, "int"))
      node->type = TYPE_INT;
    else
      node->type = TYPE_FLOAT;
    // id & num
    // 由不同的表达式填充
    if (n->children_num == 3) {
      // 变量声明
      // 获取变量名称
      node->id = n->children[1]->name;
    } else if (n->children_num == 6) {
      // 数组声明
      // 获取数组名称
      node->id = n->children[1]->name;
      // 将描述数组大小的字符串转换为整数
      int num = std::stoi(n->children[3]->name);
      // 创建一个结点用于存放数组大小
      auto num_node = std::make_shared<ASTNum>();
      num_node->i_val = num;
      num_node->type = TYPE_INT;
      node->num = num_node;
    } else {
      std::cerr << "[ast]: var-declaration transform failure!" << std::endl;
      std::abort();
    }
    return node;
  } else if (_STR_EQ(n->name, "fun-declaration")) {
    // fun-declaration -> type-specifier ID ( params ) compound-stmt
    // 由表达式和 ASTFunDeclaration的结构，我们需要填充
    // type, id, params, compound_stmt 这四个字段
    auto node = new ASTFunDeclaration();
    // 处理 type 字段
    if (_STR_EQ(n->children[0]->children[0]->name, "int")) {
      node->type = TYPE_INT;
    } else if (_STR_EQ(n->children[0]->children[0]->name, "float")) {
      node->type = TYPE_FLOAT;
    } else {
      node->type = TYPE_VOID;
    }
    // 处理 id 字段
    node->id = n->children[1]->name;

    // 处理 params 字段
    std::stack<syntax_tree_node *> s;

    auto list_ptr = n->children[3]->children[0];
    if (list_ptr->children_num != 0) {
      // 如果孩子数为0，说明没有参数，为void类型
      if (list_ptr->children_num == 3) {
        while (list_ptr->children_num == 3) {
          // 如果孩子数是3，说明是 param , param-list 结构，将param入栈
          s.push(list_ptr->children[2]);
          list_ptr = list_ptr->children[0];
        }
      }
      // 处理最后一个param节点
      s.push(list_ptr->children[0]);

      while (!s.empty()) {
        // 处理栈顶的param节点，转换为ASTParam节点
        auto child_node = static_cast<ASTParam *>(transform_node_iter(s.top()));
        //  使用shared_ptr管理内存
        auto child_node_shared = std::shared_ptr<ASTParam>(child_node);
        // 添加到fun-declaration的params列表中
        node->params.push_back(child_node_shared);
        // 弹出栈顶的param节点
        s.pop();
      }
    }

    // 处理 compound-stmt
    auto stmt_node =
        static_cast<ASTCompoundStmt *>(transform_node_iter(n->children[5]));
    // 使用shared_ptr管理内存
    node->compound_stmt = std::shared_ptr<ASTCompoundStmt>(stmt_node);
    return node;
  } else if (_STR_EQ(n->name, "param")) {
    // param -> type-specifier ID | type-specifier ID [ ]
    // 如int a 或 float b[]
    // ASTParam的结构 主要需要填充的属性有 type, id, isarray
    auto node = new ASTParam();
    if (_STR_EQ(n->children[0]->children[0]->name, "int"))
      node->type = TYPE_INT;
    else
      node->type = TYPE_FLOAT;
    // 设置参数的名称
    node->id = n->children[1]->name;
    if (n->children_num > 2)
    // 根据使用的产生式长度判断是否为数组参数
      node->isarray = true;
    return node;
  } else if (_STR_EQ(n->name, "compound-stmt")) {
    auto node = new ASTCompoundStmt();
    // 处理local_declarations
    if (n->children[1]->children_num == 2) {
      // 有局部变量声明，需要flatten
      auto list_ptr = n->children[1];
      std::stack<syntax_tree_node *> s;
      // 将所有var-declaration的语法分析树结点入栈
      while (list_ptr->children_num == 2) {
        // 将var-declaration入栈，list_ptr=local-declarations
        // 直到local-declarations=empty
        s.push(list_ptr->children[1]);
        list_ptr = list_ptr->children[0];
      }

      // 处理局部变量声明
      while (!s.empty()) {
        // 处理栈顶的var-declaration节点，转换为ASTVarDeclaration节点
        auto decl_node =
            static_cast<ASTVarDeclaration *>(transform_node_iter(s.top()));
        auto decl_node_ptr = std::shared_ptr<ASTVarDeclaration>(decl_node);
        // 将decl_node_ptr添加到ASTCompoundStmt的local_declarations列表中
        node->local_declarations.push_back(decl_node_ptr);
        s.pop();
      }
    }
    // 处理statement-list
    if (n->children[2]->children_num == 2) {
      // 如果有语句列表，需要flatten
      // flatten statement-list
      auto list_ptr = n->children[2];
      std::stack<syntax_tree_node *> s;
      // 将所有statement的语法分析树结点入栈
      while (list_ptr->children_num == 2) {
        s.push(list_ptr->children[1]);
        list_ptr = list_ptr->children[0];
      }
      // 处理语句列表
      while (!s.empty()) {
        auto stmt_node =
            static_cast<ASTStatement *>(transform_node_iter(s.top()));
        auto stmt_node_ptr = std::shared_ptr<ASTStatement>(stmt_node);
        node->statement_list.push_back(stmt_node_ptr);
        s.pop();
      }
    }
    return node;
  } else if (_STR_EQ(n->name, "statement")) {
    // 向下传递，处理statement的具体类型
    return transform_node_iter(n->children[0]);
  } else if (_STR_EQ(n->name, "expression-stmt")) {
    auto node = new ASTExpressionStmt();
    // 表达式不为空，处理expression
    if (n->children_num == 2) {
      auto expr_node =
          static_cast<ASTExpression *>(transform_node_iter(n->children[0]));

      auto expr_node_ptr = std::shared_ptr<ASTExpression>(expr_node);
      node->expression = expr_node_ptr;
    }
    return node;
  } else if (_STR_EQ(n->name, "selection-stmt")) {
    auto node = new ASTSelectionStmt();

    // 处理expression
    auto expr_node =
        static_cast<ASTExpression *>(transform_node_iter(n->children[2]));
    auto expr_node_ptr = std::shared_ptr<ASTExpression>(expr_node);
    node->expression = expr_node_ptr;

    // 处理if-statement
    auto if_stmt_node =
        static_cast<ASTStatement *>(transform_node_iter(n->children[4]));
    auto if_stmt_node_ptr = std::shared_ptr<ASTStatement>(if_stmt_node);
    // 设置if_statement字段
    node->if_statement = if_stmt_node_ptr;

   // 检查是否有else语句
    if (n->children_num == 7) {
      auto else_stmt_node =
          static_cast<ASTStatement *>(transform_node_iter(n->children[6]));
      auto else_stmt_node_ptr = std::shared_ptr<ASTStatement>(else_stmt_node);
      // 设置else_statement字段
      node->else_statement = else_stmt_node_ptr;
    }

    return node;
  } else if (_STR_EQ(n->name, "iteration-stmt")) {
    auto node = new ASTIterationStmt();

    // 处理expression
    auto expr_node =
        static_cast<ASTExpression *>(transform_node_iter(n->children[2]));
    auto expr_node_ptr = std::shared_ptr<ASTExpression>(expr_node);
    // 设置expression字段 
    node->expression = expr_node_ptr;


    // 处理statement
    auto stmt_node =
        static_cast<ASTStatement *>(transform_node_iter(n->children[4]));
    auto stmt_node_ptr = std::shared_ptr<ASTStatement>(stmt_node);
    // 添加到iteration-stmt的statement字段
    node->statement = stmt_node_ptr;

    return node;
  } else if (_STR_EQ(n->name, "return-stmt")) {
    auto node = new ASTReturnStmt();
    // 有返回的expression，处理expression
    // 没有返回值的return语句，expression为空
    if (n->children_num == 3) {
      auto expr_node =
          static_cast<ASTExpression *>(transform_node_iter(n->children[1]));
      node->expression = std::shared_ptr<ASTExpression>(expr_node);
    }
    return node;
  } else if (_STR_EQ(n->name, "expression")) {
    // simple-expression
    if (n->children_num == 1) {
      // 如果只有一个子节点，处理simple-expression
      return transform_node_iter(n->children[0]);
    }
    // 否则是 var = simple-expression，处理赋值表达式
    auto node = new ASTAssignExpression();

    // 处理var节点
    auto var_node = static_cast<ASTVar *>(transform_node_iter(n->children[0]));
    node->var = std::shared_ptr<ASTVar>(var_node);

    // 处理simple-expression节点
    auto expr_node =
        static_cast<ASTExpression *>(transform_node_iter(n->children[2]));
    node->expression = std::shared_ptr<ASTExpression>(expr_node);

    return node;
  } else if (_STR_EQ(n->name, "var")) {
    auto node = new ASTVar();
    // 获取变量名称
    node->id = n->children[0]->name;
    // 检查是否有数组索引，如果有，处理expression
    if (n->children_num == 4) {
      auto expr_node =
          static_cast<ASTExpression *>(transform_node_iter(n->children[2]));
      node->expression = std::shared_ptr<ASTExpression>(expr_node);
    }
    return node;
  } else if (_STR_EQ(n->name, "simple-expression")) {
    auto node = new ASTSimpleExpression();
    // 处理第一个additive-expression
    auto expr_node_1 = static_cast<ASTAdditiveExpression *>(
        transform_node_iter(n->children[0]));
    node->additive_expression_l =
        std::shared_ptr<ASTAdditiveExpression>(expr_node_1);

    // 检查是否有第二个additive-expression
    if (n->children_num == 3) {
      // 获取关系运算符，并设置op字段
      auto op_name = n->children[1]->children[0]->name;
      if (_STR_EQ(op_name, "<="))
        node->op = OP_LE;
      else if (_STR_EQ(op_name, "<"))
        node->op = OP_LT;
      else if (_STR_EQ(op_name, ">"))
        node->op = OP_GT;
      else if (_STR_EQ(op_name, ">="))
        node->op = OP_GE;
      else if (_STR_EQ(op_name, "=="))
        node->op = OP_EQ;
      else if (_STR_EQ(op_name, "!="))
        node->op = OP_NEQ;

      // 处理第二个additive-expression
      auto expr_node_2 = static_cast<ASTAdditiveExpression *>(
          transform_node_iter(n->children[2]));
      node->additive_expression_r =
          std::shared_ptr<ASTAdditiveExpression>(expr_node_2);
    }
    return node;
  } else if (_STR_EQ(n->name, "additive-expression")) {
    auto node = new ASTAdditiveExpression();
    if (n->children_num == 3) {
      // 如果有两个子节点，处理additive-expression
      auto add_expr_node = static_cast<ASTAdditiveExpression *>(
          transform_node_iter(n->children[0]));
      // 设置additive_expression字段
      node->additive_expression =
          std::shared_ptr<ASTAdditiveExpression>(add_expr_node);

      // 获取加减运算符，并设置op字段
      auto op_name = n->children[1]->children[0]->name;
      if (_STR_EQ(op_name, "+"))
        node->op = OP_PLUS;
      else if (_STR_EQ(op_name, "-"))
        node->op = OP_MINUS;

      // / 处理term节点
      auto term_node =
          static_cast<ASTTerm *>(transform_node_iter(n->children[2]));
      // 设置term字段
      node->term = std::shared_ptr<ASTTerm>(term_node);
    } else {
      // 只有一个子节点，处理term节点
      auto term_node =
          static_cast<ASTTerm *>(transform_node_iter(n->children[0]));
      node->term = std::shared_ptr<ASTTerm>(term_node);
    }
    return node;
  } else if (_STR_EQ(n->name, "term")) {
    auto node = new ASTTerm();
    if (n->children_num == 3) {
      // 处理左侧的term节点
      auto term_node =
          static_cast<ASTTerm *>(transform_node_iter(n->children[0]));
      node->term = std::shared_ptr<ASTTerm>(term_node);

      // 获取乘除运算符，并设置op字段
      auto op_name = n->children[1]->children[0]->name;
      if (_STR_EQ(op_name, "*"))
        node->op = OP_MUL;
      else if (_STR_EQ(op_name, "/"))
        node->op = OP_DIV;

      // 处理右侧的factor节点
      auto factor_node =
          static_cast<ASTFactor *>(transform_node_iter(n->children[2]));
      node->factor = std::shared_ptr<ASTFactor>(factor_node);
    } else {
      // 只有一个子节点，处理factor节点
      auto factor_node =
          static_cast<ASTFactor *>(transform_node_iter(n->children[0]));
      node->factor = std::shared_ptr<ASTFactor>(factor_node);
    }
    return node;
  } else if (_STR_EQ(n->name, "factor")) {
    int i = 0;
    if (n->children_num == 3)
      i = 1;
    // 获取子节点的名称，判断是哪种factor类型
    auto name = n->children[i]->name;
    if (_STR_EQ(name, "expression") || _STR_EQ(name, "var") ||
        _STR_EQ(name, "call"))
        // 如果是expression, var, call类型，递归转换该子节点
      return transform_node_iter(n->children[i]);
    else {
      // 否则是num类型，创建ASTNum节点，包含类型和数值
      auto num_node = new ASTNum();
      if (_STR_EQ(name, "integer")) {
        num_node->type = TYPE_INT;
        num_node->i_val = std::stoi(n->children[i]->children[0]->name);
      } else if (_STR_EQ(name, "float")) {
        num_node->type = TYPE_FLOAT;
        num_node->f_val = std::stof(n->children[i]->children[0]->name);
      } else {
        _AST_NODE_ERROR_
      }
      return num_node;
    }
  } else if (_STR_EQ(n->name, "call")) {
    auto node = new ASTCall();
    node->id = n->children[0]->name;
    // flatten args
    if (_STR_EQ(n->children[2]->children[0]->name, "arg-list")) {
      // 指向参数列表
      auto list_ptr = n->children[2]->children[0];
      auto s = std::stack<syntax_tree_node *>();
      // 将所有expression节点入栈
      while (list_ptr->children_num == 3) {
        s.push(list_ptr->children[2]);
        list_ptr = list_ptr->children[0];
      }
      s.push(list_ptr->children[0]);

      // 循环处理栈中的expression节点，创建ASTExpression节点并添加到args列表中
      while (!s.empty()) {
        // 处理栈顶的expression节点
        auto expr_node =
            static_cast<ASTExpression *>(transform_node_iter(s.top()));
        auto expr_node_ptr = std::shared_ptr<ASTExpression>(expr_node);
        node->args.push_back(expr_node_ptr);
        // 弹出栈顶的expression节点
        s.pop();
      }
    }
    return node;
  } else {
    std::cerr << "[ast]: transform failure!" << std::endl;
    std::abort();
  }
}

Value* ASTProgram::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTNum::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTVarDeclaration::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTFunDeclaration::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTParam::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTCompoundStmt::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTExpressionStmt::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTSelectionStmt::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTIterationStmt::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTReturnStmt::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTAssignExpression::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTSimpleExpression::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTAdditiveExpression::accept(ASTVisitor &visitor) {
    return visitor.visit(*this);
}
Value* ASTVar::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTTerm::accept(ASTVisitor &visitor) { return visitor.visit(*this); }
Value* ASTCall::accept(ASTVisitor &visitor) { return visitor.visit(*this); }

#define _DEBUG_PRINT_N_(N)                                                     \
    { std::cout << std::string(N, '-'); }

Value* ASTPrinter::visit(ASTProgram &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "program" << std::endl;
    add_depth();
    for (auto decl : node.declarations) {
        decl->accept(*this);
    }
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTNum &node) {
    _DEBUG_PRINT_N_(depth);
    if (node.type == TYPE_INT) {
        std::cout << "num (int): " << node.i_val << std::endl;
    } else if (node.type == TYPE_FLOAT) {
        std::cout << "num (float): " << node.f_val << std::endl;
    } else {
        _AST_NODE_ERROR_
    }
    return nullptr;
}

Value* ASTPrinter::visit(ASTVarDeclaration &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "var-declaration: " << node.id;
    if (node.num != nullptr) {
        std::cout << "[]" << std::endl;
        add_depth();
        node.num->accept(*this);
        remove_depth();
        return nullptr;
    }
    std::cout << std::endl;
    return nullptr;
}

Value* ASTPrinter::visit(ASTFunDeclaration &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "fun-declaration: " << node.id << std::endl;
    add_depth();
    for (auto param : node.params) {
        param->accept(*this);
    }

    node.compound_stmt->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTParam &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "param: " << node.id;
    if (node.isarray)
        std::cout << "[]";
    std::cout << std::endl;
    return nullptr;
}

Value* ASTPrinter::visit(ASTCompoundStmt &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "compound-stmt" << std::endl;
    add_depth();
    for (auto decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto stmt : node.statement_list) {
        stmt->accept(*this);
    }
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTExpressionStmt &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "expression-stmt" << std::endl;
    add_depth();
    if (node.expression != nullptr)
        node.expression->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTSelectionStmt &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "selection-stmt" << std::endl;
    add_depth();
    node.expression->accept(*this);
    node.if_statement->accept(*this);
    if (node.else_statement != nullptr)
        node.else_statement->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTIterationStmt &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "iteration-stmt" << std::endl;
    add_depth();
    node.expression->accept(*this);
    node.statement->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTReturnStmt &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "return-stmt";
    if (node.expression == nullptr) {
        std::cout << ": void" << std::endl;
    } else {
        std::cout << std::endl;
        add_depth();
        node.expression->accept(*this);
        remove_depth();
    }
    return nullptr;
}

Value* ASTPrinter::visit(ASTAssignExpression &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "assign-expression" << std::endl;
    add_depth();
    node.var->accept(*this);
    node.expression->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTSimpleExpression &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "simple-expression";
    if (node.additive_expression_r == nullptr) {
        std::cout << std::endl;
    } else {
        std::cout << ": ";
        if (node.op == OP_LT) {
            std::cout << "<";
        } else if (node.op == OP_LE) {
            std::cout << "<=";
        } else if (node.op == OP_GE) {
            std::cout << ">=";
        } else if (node.op == OP_GT) {
            std::cout << ">";
        } else if (node.op == OP_EQ) {
            std::cout << "==";
        } else if (node.op == OP_NEQ) {
            std::cout << "!=";
        } else {
            std::abort();
        }
        std::cout << std::endl;
    }
    add_depth();
    node.additive_expression_l->accept(*this);
    if (node.additive_expression_r != nullptr)
        node.additive_expression_r->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTAdditiveExpression &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "additive-expression";
    if (node.additive_expression == nullptr) {
        std::cout << std::endl;
    } else {
        std::cout << ": ";
        if (node.op == OP_PLUS) {
            std::cout << "+";
        } else if (node.op == OP_MINUS) {
            std::cout << "-";
        } else {
            std::abort();
        }
        std::cout << std::endl;
    }
    add_depth();
    if (node.additive_expression != nullptr)
        node.additive_expression->accept(*this);
    node.term->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTVar &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "var: " << node.id;
    if (node.expression != nullptr) {
        std::cout << "[]" << std::endl;
        add_depth();
        node.expression->accept(*this);
        remove_depth();
        return nullptr;
    }
    std::cout << std::endl;
    return nullptr;
}

Value* ASTPrinter::visit(ASTTerm &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "term";
    if (node.term == nullptr) {
        std::cout << std::endl;
    } else {
        std::cout << ": ";
        if (node.op == OP_MUL) {
            std::cout << "*";
        } else if (node.op == OP_DIV) {
            std::cout << "/";
        } else {
            std::abort();
        }
        std::cout << std::endl;
    }
    add_depth();
    if (node.term != nullptr)
        node.term->accept(*this);

    node.factor->accept(*this);
    remove_depth();
    return nullptr;
}

Value* ASTPrinter::visit(ASTCall &node) {
    _DEBUG_PRINT_N_(depth);
    std::cout << "call: " << node.id << "()" << std::endl;
    add_depth();
    for (auto arg : node.args) {
        arg->accept(*this);
    }
    remove_depth();
    return nullptr;
}
