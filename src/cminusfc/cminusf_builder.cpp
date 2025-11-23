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
    // 如果两个类型相同
    if (l_val->get_type() == r_val->get_type()) {
        // 如果两个类型相同，返回 true if integer type
        is_int = l_val->get_type()->is_integer_type();
    } else {
        // 不同类型时，进行类型提升，提升后均为浮点型，返回 false
        if (l_val->get_type()->is_integer_type()) {
            l_val = builder->create_sitofp(l_val, FLOAT_T);
        } else {
            r_val = builder->create_sitofp(r_val, FLOAT_T);
        }
    }
    // 返回 true if integer type
    return is_int;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

 // 处理所有declarations
 // 1. 遍历declarations列表，处理每个declaration节点
 // 2. 返回最后一个declaration处理的返回值
Value* CminusfBuilder::visit(ASTProgram &node) {
    // 从模块中获取常用类型
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

// 处理数字字面量
Value* CminusfBuilder::visit(ASTNum &node) {
    if (node.type == TYPE_INT) {
        // 返回指向整型常量的指针
        return CONST_INT(node.i_val);
    }
    // 返回指向浮点型常量的指针
    return CONST_FP(node.f_val);
}

// 处理变量声明语句
// 1. 根据变量类型和是否为数组创建相应的类型
// 2. 在全局或局部作用域中分配变量空间
// 3. 初始化变量为0
// 4. 将变量添加到作用域中
// 5. 返回分配的变量空间指针
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
        }else {
            // 如果不是常量，报错
            assert(false && "Array size must be constant");
        }
        
        // 创建数组类型
        auto array_type = ArrayType::get(var_type, array_size);
        
        if (scope.in_global()) {
            // 使用ConstantZero初始化全局数组变量
            Constant* init_val = ConstantZero::get(array_type, module.get());
            alloca = GlobalVariable::create(name, module.get(), array_type, false, init_val);
        }
        else {
            // 局部数组变量
            alloca = builder->create_alloca(array_type);
            // 局部数组初始化为0
            for (int i = 0; i < array_size; i++) {
                // 创建gep获取数组元素的指针
                auto element_ptr = builder->create_gep(alloca, {CONST_INT(0), CONST_INT(i)});
                if (node.type == TYPE_INT) {
                    builder->create_store(CONST_INT(0), element_ptr);
                } else {
                    builder->create_store(CONST_FP(0.0f), element_ptr);
                }
            }
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

// 处理函数定义语句
// 1. 根据返回值类型（只能为int，float和void）创建函数类型
// 2. 根据函数参数列表和返回值类型创建函数
// 3. 将函数添加到作用域中
// 4. 创建函数的入口基本块
// 5. 处理函数参数，分配栈上空间并存储参数
// 6. 处理函数体
// 7. 如果函数没有返回语句，添加默认返回语句
// 8. 退出函数作用域
Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    // 函数参数类型列表
    std::vector<Type *> param_types;
    // 获取函数返回值
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    // 根据AST结点中数据，创建函数参数列表
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
    // 根据参数列表和函数返回值类型获取函数类型
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
    // 获取函数的参数列表
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        // 调用每个参数的accept方法，处理函数参数，为每个函数的参数分配栈上空间，并返回分配的内存地址param_i
        auto* param_i = node.params[i]->accept(*this);
        // 将IR中的参数（如%arg0）重命名为AST中的参数名
        args[i]->set_name(node.params[i]->id);
        // 将参数值存储到分配的内存地址中
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

// 处理函数参数
// 1. 根据参数类型分配栈上空间
// 2. 返回分配的内存地址
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
    // 创建一条栈上分配内存指令
    auto alloca=builder->create_alloca(param_type);
    // 返回分配的内存地址
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
        // 如果当前基本块已经终止了，且仍在当前基本块中插入代码，则创建一个新的基本块继续插入
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

// 处理选择语句
// 1. 处理条件表达式，获取条件值
// 2. 创建基本块用于if分支，else分支（如果有）
// 3. 创建基本块用于if-else语句后的继续执行
// 4. 根据条件值创建条件跳转指令
// 5. 处理if语句块
// 6. 如果有else语句块，处理else语句块
// 7. 设置插入点到继续执行的基本块
// 8. 返回nullptr
Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    // 表达式返回的条件值
    auto *ret_val = node.expression->accept(*this);
    // 创建if分支和else分支的基本块，以及继续执行的基本块
    auto *trueBB = BasicBlock::create(module.get(), "", context.func);
    BasicBlock *falseBB{};
    // 创建继续执行基本块
    auto *contBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val = nullptr;
    // 根据返回值类型，选择合适的比较指令
    if (ret_val->get_type()->is_integer_type()) {
        // 创建整数比较指令，cond_val为布尔类型的条件值
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        // 创建浮点数比较指令，cond_val为布尔类型的条件值
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

// 处理变量访问
// 1. 在作用域中查找变量地址
// 2. 获取变量类型，可能是基本类型或数组类型
// 3. 如果有下标表达式，处理下标表达式，进行数组访问
// 4. 根据context中require_lvalue标志，决定返回变量地址或加载变量值
Value* CminusfBuilder::visit(ASTVar &node) {
    // 在作用域中根据名称查找变量地址
    Value* baseAddr = this->scope.find(node.id);
    // 获取变量类型，可能是基本类型或数组类型
    Type* alloctype = nullptr;
    
    if(baseAddr->is<AllocaInst>()) {
        // 如果是栈上分配的变量，获取其类型
        alloctype = baseAddr->as<AllocaInst>()->get_alloca_type();
    } else {
        // 如果是全局变量，获取其类型
        alloctype = baseAddr->as<GlobalVariable>()->get_type()->get_pointer_element_type();
    }

    // 如果有下标表达式，处理下标表达式，进行数组访问
    if(node.expression) {
        bool original_require_lvalue = context.require_lvalue;
        // 因为表达式可能嵌套，所以先设置为不需要左值，避免递归时错误，等递归结束后再恢复
        context.require_lvalue = false;
        // 处理下标表达式
        auto idx = node.expression->accept(*this);
        context.require_lvalue = original_require_lvalue;

        // 类型转换，确保下标是整型
        if (idx->get_type()->is_float_type()) {
            idx = builder->create_fptosi(idx, INT32_T);
        } else if(idx->get_type()->is_int1_type()){
            idx = builder->create_zext(idx, INT32_T);
        }
        auto right_bb = BasicBlock::create(module.get(), "", context.func);
        auto wrong_bb = BasicBlock::create(module.get(), "", context.func);
        
        // 生成检查下标是否为负数的代码
        auto cond_neg = builder->create_icmp_ge(idx, CONST_INT(0));
        builder->create_cond_br(cond_neg,right_bb, wrong_bb);

        // 查找异常处理函数
        auto wrong = scope.find("neg_idx_except");
        // 设置插入点为wrong_bb，向wrong_bb中插入调用异常处理函数的代码
        builder->set_insert_point(wrong_bb);
        // 插入调用异常处理函数的指令
        builder->create_call(wrong, {});
        // 插入一条br指令，跳转到right_bb
        builder->create_br(right_bb);
        // 设置插入点到right_bb
        builder->set_insert_point(right_bb);
        
        // 需要左值的情况，即赋值语句
        if(context.require_lvalue) {
            if(alloctype->is_pointer_type()) {
                // 将栈上分配的指针加载出来
                baseAddr = builder->create_load(baseAddr);
                // 根据下标在数组中计算元素地址
                baseAddr = builder->create_gep(baseAddr,{idx});
            } else if(alloctype->is_array_type()){ 
                // 如果是数组类型，计算元素地址
                baseAddr = builder->create_gep(baseAddr,{CONST_INT(0),idx});
            }
            // 处理完设置require_lvalue为false
            context.require_lvalue = false;
            // 返回元素地址
            return baseAddr;
        } else {
            if(alloctype->is_pointer_type()){
                baseAddr = builder->create_load(baseAddr);
                baseAddr = builder->create_gep(baseAddr,{idx});
            } else if(alloctype->is_array_type()){ 
                baseAddr = builder->create_gep(baseAddr,{CONST_INT(0),idx});
            }
            // 如果不需要左值，加载元素值并返回
            // 例如在表达式中使用数组元素arr[i]
            baseAddr = builder->create_load(baseAddr);
            return baseAddr;
        }
    } else {
        // 处理普通变量访问
        if (context.require_lvalue) {
            // 设置require_lvalue为false
            context.require_lvalue = false;
            // 返回变量地址
            return baseAddr;
        } else {
            // if(alloctype->is_array_type()){
            //     return builder->create_gep(baseAddr, {CONST_INT(0),CONST_INT(0)});
            // } else {
            //     return builder->create_load(baseAddr);
            // }
            return builder->create_load(baseAddr);
        }
    }
    return nullptr;
}

// 处理赋值表达式
Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    // 递归处理，最后一定是简单表达式
    // 获取简单表达式的值
    auto *expr_result = node.expression->accept(*this);
    // context中标记需要左值
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
    // 如果加法表达式没有左侧表达式，直接处理右侧项
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
    // 如果乘法表达式没有左侧表达式，直接处理右侧项
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

// 处理函数调用语句
// 1. 获取函数指针
// 2. 处理参数列表
// 3. 创建函数调用指令
Value* CminusfBuilder::visit(ASTCall &node) {
    // 在作用域中根据函数名查找函数
    auto *func = dynamic_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto param_type = func->get_function_type()->param_begin();
    for (auto &arg : node.args) {
        auto *arg_val = arg->accept(*this);
        // 如果参数类型不一致且不是指针类型，则进行类型转换
        // 如果是指针类型，则不进行类型转换，直接传递参数
        if (!arg_val->get_type()->is_pointer_type() &&
            *param_type != arg_val->get_type()) {
            if (arg_val->get_type()->is_integer_type()) {
                arg_val = builder->create_sitofp(arg_val, FLOAT_T);
            } else {
                arg_val = builder->create_fptosi(arg_val, INT32_T);
            }
        }
        args.push_back(arg_val);
        // 迭代器移动到下一个参数
        param_type++;
    }

    // 创建函数调用指令
    return builder->create_call(static_cast<Function *>(func), args);
}
