#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

bool promote(IRBuilder *builder, Value **l_val_p, Value **r_val_p) {
    bool is_int = false;
    auto &l_val = *l_val_p;
    auto &r_val = *r_val_p;
    if (l_val->get_type() == r_val->get_type()) {
        is_int = l_val->get_type()->is_integer_type();
    } else {
        if (l_val->get_type()->is_integer_type()) {
            l_val = builder->create_sitofp(l_val, FLOAT_T);
        } else {
            r_val = builder->create_sitofp(r_val, FLOAT_T);
        }
    }
    return is_int;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    if (node.type == TYPE_INT) {
        return CONST_INT(node.i_val);
    }
    return CONST_FP(node.f_val);
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // TODO: This function is empty now.
    // Add some code here.
    
    // 获取变量名称
    std::string name=node.id;
    Type* var_type=nullptr;
    Value*  alloca=nullptr;

    //判断变量类型
    if (node.type == TYPE_INT) {
        var_type = INT32_T;
    } else {
        var_type = FLOAT_T;
    }

    // 如果是数组声明
    if (node.num != nullptr) {
        // 获取数组大小
        auto array_size_val = node.num->accept(*this);
        unsigned array_size = 0;
        
        // 从常量中提取数组大小
        if (auto const_int = dynamic_cast<ConstantInt*>(array_size_val)) {
            array_size = const_int->get_value();
        }
        
        // 创建数组类型
        auto array_type = ArrayType::get(var_type, array_size);
        
        if (scope.in_global()) {
            // 全局数组变量
            std::vector<Constant*> init_vals(array_size);
            Constant* init_val = nullptr;
            
            if (node.type == TYPE_INT) {
                init_val = CONST_INT(0);
            } else {
                init_val = CONST_FP(0.0f);
            }
            
            for (unsigned i = 0; i < array_size; i++) {
                init_vals[i] = init_val;
            }
            
            auto const_array = ConstantArray::get(array_type, init_vals);
            alloca = GlobalVariable::create(name, module.get(), array_type, false, const_array);
        }
        else {
            // 局部数组变量
            alloca = builder->create_alloca(array_type);
        }
    }
    else {
        // 普通变量声明
        if (scope.in_global()) {
            // 全局变量
            Constant* init_val = nullptr;
            if (node.type == TYPE_INT) {
                init_val = CONST_INT(0);
            } else {
                init_val = CONST_FP(0.0f);
            }
            alloca = GlobalVariable::create(name, module.get(), var_type, false, init_val);
        } else {
            // 局部变量
            alloca = builder->create_alloca(var_type);
            
            // 局部变量初始化为0
            if (node.type == TYPE_INT) {
                builder->create_store(CONST_INT(0), alloca);
            } else {
                builder->create_store(CONST_FP(0.0f), alloca);
            }
        }
    }
    
    // 将变量添加到作用域
    scope.push(name, alloca);
    return alloca;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    // 获取函数返回值
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    // 获取函数参数列表
    for (auto &param : node.params) {
        if (param->type == TYPE_INT) {
            if (param->isarray) {
                param_types.push_back(INT32PTR_T);
            } else {
                param_types.push_back(INT32_T);
            }
        } else {
            if (param->isarray) {
                param_types.push_back(FLOATPTR_T);
            } else {
                param_types.push_back(FLOAT_T);
            }
        }
    }

    // 获取函数类型
    fun_type = FunctionType::get(ret_type, param_types);
    // 创建函数
    auto func = Function::create(fun_type, node.id, module.get());
    // 将刚创建的函数添加到作用域中
    scope.push(node.id, func);
    // 在context中存储当前处理的函数
    context.func = func;
    // 创建基本块
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    // 设置代码插入位置
    builder->set_insert_point(funBB);
    // 进入函数的作用域
    scope.enter();
    context.pre_enter_scope = true;
    std::vector<Value *> args;
    // 获取函数的参数
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        auto* param_i = node.params[i]->accept(*this);
        // 将AST中的变量名赋值给args[i]
        args[i]->set_name(node.params[i]->id);
        // 创建指令
        builder->create_store(args[i], param_i);
        // 将参数名和内存地址添加到作用域中
        scope.push(args[i]->get_name(), param_i);
    }
    // 处理复合语句
    node.compound_stmt->accept(*this);
    // 如果当前基本块基本块不进行跳转操作
    if (builder->get_insert_block()->get_terminator() == nullptr) 
    {
        // 添加函数返回值语句
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    // 退出函数的作用域
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    // 这段代码应该实现为函数参数分配栈上空间，测试案例一直是无参数，所以不会被调用
    // 获取对应的LLVM类型，参数只有int和float两种类型，无void型
    Type * param_type;
    if (node.type == TYPE_INT)
        param_type = INT32_T;
    else{
        param_type = FLOAT_T;
    }
    // 如果参数是数组类型，则转换为指针类型
    if (node.isarray) {
        if (node.type == TYPE_INT) {
            param_type = INT32PTR_T;
        } else {
            param_type = FLOATPTR_T;
        }
    }
    auto alloca=builder->create_alloca(param_type);
    return alloca;
}

// 处理复合语句
Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    // TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements. 
    
    // 进入新的作用域
    scope.enter();
    
    // 保存外层的控制流目标
    BasicBlock *outer_break = context.break_target;
    BasicBlock *outer_continue = context.continue_target;
    
    // 重置当前的控制流目标（复合语句内部可能有自己的循环）
    context.break_target = nullptr;
    context.continue_target = nullptr;
    
    // 处理局部变量声明
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }
    
    // 处理语句列表
    BasicBlock *current_bb = builder->get_insert_block();
    for (auto &stmt : node.statement_list) {
        // 如果前一个语句终止了基本块，需要创建新的基本块
        if (current_bb->is_terminated() && current_bb == builder->get_insert_block()) {
            auto new_bb = BasicBlock::create(module.get(), "", context.func);
            builder->set_insert_point(new_bb);
            current_bb = new_bb;
        }
        
        // 处理当前语句
        stmt->accept(*this);
        
        // 更新当前基本块引用
        current_bb = builder->get_insert_block();
        
        // 如果遇到break或continue，提前退出
        if (context.break_target || context.continue_target) {
            break;
        }
    }
    // 恢复外层的控制流目标
    context.break_target = outer_break;
    context.continue_target = outer_continue;
    
    // 退出作用域
    scope.exit();

    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    if (node.expression != nullptr) {
        return node.expression->accept(*this);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    auto *ret_val = node.expression->accept(*this);
    auto *trueBB = BasicBlock::create(module.get(), "", context.func);
    BasicBlock *falseBB{};
    auto *contBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val = nullptr;
    if (ret_val->get_type()->is_integer_type()) {
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));
    }

    if (node.else_statement == nullptr) {
        builder->create_cond_br(cond_val, trueBB, contBB);
    } else {
        falseBB = BasicBlock::create(module.get(), "", context.func);
        builder->create_cond_br(cond_val, trueBB, falseBB);
    }
    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);

    if (not builder->get_insert_block()->is_terminated()) {
        builder->create_br(contBB);
    }

    if (node.else_statement == nullptr) {
        // falseBB->erase_from_parent(); // did not clean up memory
    } else {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (not builder->get_insert_block()->is_terminated()) {
            builder->create_br(contBB);
        }
    }

    builder->set_insert_point(contBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.

    
    // 获取当前函数
    Function *current_func = context.func;

    // 生成唯一的基本块标签
    static int while_counter = 0;
    std::string prefix = "while_" + std::to_string(while_counter++);
    
    auto *cond_bb = BasicBlock::create(module.get(), prefix + "_cond", current_func);
    auto *body_bb = BasicBlock::create(module.get(), prefix + "_body", current_func);
    auto *end_bb = BasicBlock::create(module.get(), prefix + "_end", current_func);
    
    // 保存外层的控制流目标
    BasicBlock *outer_break_target = context.break_target;
    BasicBlock *outer_continue_target = context.continue_target;
    
    // 设置当前循环的控制流目标
    context.break_target = end_bb;        // break 跳转到循环结束
    context.continue_target = cond_bb;    // continue 跳转到条件判断

    // 生成跳转到条件判断的指令
    builder->create_br(cond_bb);
    
    // 设置插入点到条件判断基本块
    builder->set_insert_point(cond_bb);
    
    // 处理循环条件表达式
    Value *cond_value = node.expression->accept(*this);

    // 确保条件值是布尔类型
    if (cond_value->get_type()->is_integer_type()) {
        // 对于整数类型，与0比较
        if (cond_value->get_type()->is_int32_type()) {
            cond_value = builder->create_icmp_ne(cond_value, CONST_INT(0));
        } else if (cond_value->get_type()->is_int1_type()) {
            // 已经是布尔类型，无需转换
        }
    } else if (cond_value->get_type()->is_float_type()) {
        // 对于浮点类型，与0.0比较
        cond_value = builder->create_fcmp_ne(cond_value, CONST_FP(0.0f));
    }

    // 根据条件值跳转到循环体或结束
    builder->create_cond_br(cond_value, body_bb, end_bb);
    
    // 设置插入点到循环体基本块
    builder->set_insert_point(body_bb);
    
    // 处理循环体语句
    node.statement->accept(*this);
    
    // 如果循环体没有终止（没有return/break等），生成跳转回条件判断的指令
    if (!builder->get_insert_block()->is_terminated()) {
        builder->create_br(cond_bb);
    }

    // 设置插入点到循环结束基本块
    builder->set_insert_point(end_bb);
    
    // 恢复外层的控制流目标
    context.break_target = outer_break_target;
    context.continue_target = outer_continue_target;
    
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        auto *fun_ret_type =
            context.func->get_function_type()->get_return_type();
        auto *ret_val = node.expression->accept(*this);
        if (fun_ret_type != ret_val->get_type()) {
            if (fun_ret_type->is_integer_type()) {
                ret_val = builder->create_fptosi(ret_val, INT32_T);
            } else {
                ret_val = builder->create_sitofp(ret_val, FLOAT_T);
            }
        }

        builder->create_ret(ret_val);
    }

    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    Value* baseAddr = this->scope.find(node.id);
    Type* alloctype = nullptr;
    
    if(baseAddr->is<AllocaInst>()) {
        alloctype = baseAddr->as<AllocaInst>()->get_alloca_type();
    } else {
        alloctype = baseAddr->as<GlobalVariable>()->get_type()->get_pointer_element_type();
    }

    if(node.expression) {
        bool original_require_lvalue = context.require_lvalue;
        context.require_lvalue = false;
        auto idx = node.expression->accept(*this);
        context.require_lvalue = original_require_lvalue;

        if (idx->get_type()->is_float_type()) {
            idx = builder->create_fptosi(idx, INT32_T);
        } else if(idx->get_type()->is_int1_type()){
            idx = builder->create_zext(idx, INT32_T);
        }
        auto right_bb = BasicBlock::create(module.get(), "", context.func);
        auto wrong_bb = BasicBlock::create(module.get(), "", context.func);
        
        auto cond_neg = builder->create_icmp_ge(idx, CONST_INT(0));
        builder->create_cond_br(cond_neg,right_bb, wrong_bb);

        auto wrong = scope.find("neg_idx_except");
        builder->set_insert_point(wrong_bb);
        builder->create_call(wrong, {});
        builder->create_br(right_bb);
        builder->set_insert_point(right_bb);
        
        if(context.require_lvalue) {
            if(alloctype->is_pointer_type()) {
                baseAddr = builder->create_load(baseAddr);
                baseAddr = builder->create_gep(baseAddr,{idx});
            } else if(alloctype->is_array_type()){ 
                baseAddr = builder->create_gep(baseAddr,{CONST_INT(0),idx});
            }
            context.require_lvalue = false;
            return baseAddr;
        } else {
            if(alloctype->is_pointer_type()){
                baseAddr = builder->create_load(baseAddr);
                baseAddr = builder->create_gep(baseAddr,{idx});
            } else if(alloctype->is_array_type()){ 
                baseAddr = builder->create_gep(baseAddr,{CONST_INT(0),idx});
            }
            baseAddr = builder->create_load(baseAddr);
            return baseAddr;
        }
    } else {
        if (context.require_lvalue) {
            context.require_lvalue = false;
            return baseAddr;
            // return builder->create_gep(baseAddr, {CONST_INT(0)});
        } else {
            if(alloctype->is_array_type()){
                return builder->create_gep(baseAddr, {CONST_INT(0),CONST_INT(0)});
            } else {
                return builder->create_load(baseAddr);
            }
            
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    auto *expr_result = node.expression->accept(*this);
    context.require_lvalue = true;
    auto *var_addr = node.var->accept(*this);
    if (var_addr->get_type()->get_pointer_element_type() !=
        expr_result->get_type()) {
        if (expr_result->get_type() == INT32_T) {
            expr_result = builder->create_sitofp(expr_result, FLOAT_T);
        } else {
            expr_result = builder->create_fptosi(expr_result, INT32_T);
        }
    }
    builder->create_store(expr_result, var_addr);
    return expr_result;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    // 处理左侧加法表达式
    Value *left_val = node.additive_expression_l->accept(*this);
    
    // 如果没有右侧表达式，直接返回左侧值
    if (node.additive_expression_r == nullptr) {
        return left_val;
    }
    
    // 处理右侧加法表达式
    Value *right_val = node.additive_expression_r->accept(*this);
    
    // 类型提升：确保左右操作数类型一致
    bool is_int_type = promote(&*builder, &left_val, &right_val);
    
    Value *result = nullptr;

    // 根据操作符生成对应的比较指令
    switch (node.op) {
        case OP_LT:   // <
            if (is_int_type) {
                result = builder->create_icmp_lt(left_val, right_val);
            } else {
                result = builder->create_fcmp_lt(left_val, right_val);
            }
            break;
            
        case OP_LE:   // <=
            if (is_int_type) {
                result = builder->create_icmp_le(left_val, right_val);
            } else {
                result = builder->create_fcmp_le(left_val, right_val);
            }
            break;
            
        case OP_GT:   // >
            if (is_int_type) {
                result = builder->create_icmp_gt(left_val, right_val);
            } else {
                result = builder->create_fcmp_gt(left_val, right_val);
            }
            break;
            
        case OP_GE:   // >=
            if (is_int_type) {
                result = builder->create_icmp_ge(left_val, right_val);
            } else {
                result = builder->create_fcmp_ge(left_val, right_val);
            }
            break;
            
        case OP_EQ:   // ==
            if (is_int_type) {
                result = builder->create_icmp_eq(left_val, right_val);
            } else {
                result = builder->create_fcmp_eq(left_val, right_val);
            }
            break;
            
        case OP_NEQ:  // !=
            if (is_int_type) {
                result = builder->create_icmp_ne(left_val, right_val);
            } else {
                result = builder->create_fcmp_ne(left_val, right_val);
            }
            break;
            
        default:
            // 报错
            assert(false && "Unknown relational operator");
            break;
    }

    // 将布尔结果转换为整数，因为output函数期望整数参数
    if (result->get_type()->is_int1_type()) {
        result = builder->create_zext(result, INT32_T);
    }
    return result;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    if (node.additive_expression == nullptr) {
        return node.term->accept(*this);
    }

    auto *l_val = node.additive_expression->accept(*this);
    auto *r_val = node.term->accept(*this);
    bool is_int = promote(&*builder, &l_val, &r_val);
    Value *ret_val = nullptr;
    switch (node.op) {
    case OP_PLUS:
        if (is_int) {
            ret_val = builder->create_iadd(l_val, r_val);
        } else {
            ret_val = builder->create_fadd(l_val, r_val);
        }
        break;
    case OP_MINUS:
        if (is_int) {
            ret_val = builder->create_isub(l_val, r_val);
        } else {
            ret_val = builder->create_fsub(l_val, r_val);
        }
        break;
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    if (node.term == nullptr) {
        return node.factor->accept(*this);
    }

    auto *l_val = node.term->accept(*this);
    auto *r_val = node.factor->accept(*this);
    bool is_int = promote(&*builder, &l_val, &r_val);

    Value *ret_val = nullptr;
    switch (node.op) {
    case OP_MUL:
        if (is_int) {
            ret_val = builder->create_imul(l_val, r_val);
        } else {
            ret_val = builder->create_fmul(l_val, r_val);
        }
        break;
    case OP_DIV:
        if (is_int) {
            ret_val = builder->create_isdiv(l_val, r_val);
        } else {
            ret_val = builder->create_fdiv(l_val, r_val);
        }
        break;
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    // 在作用域中根据函数名查找函数
    auto *func = dynamic_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto param_type = func->get_function_type()->param_begin();
    for (auto &arg : node.args) {
        auto *arg_val = arg->accept(*this);
        if (!arg_val->get_type()->is_pointer_type() &&
            *param_type != arg_val->get_type()) {
            if (arg_val->get_type()->is_integer_type()) {
                arg_val = builder->create_sitofp(arg_val, FLOAT_T);
            } else {
                arg_val = builder->create_fptosi(arg_val, INT32_T);
            }
        }
        args.push_back(arg_val);
        param_type++;
    }

    return builder->create_call(static_cast<Function *>(func), args);
}
