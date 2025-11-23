#include "DeadCode.hpp"
#include "Instruction.hpp"
#include "logging.hpp"
#include <memory>
#include <vector>


// 处理流程：两趟处理，mark 标记有用变量，sweep 删除无用指令
void DeadCode::run() {
    bool changed{};
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = 0;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb1 : func->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().empty() && bb != func->get_entry_block()) {
            to_erase.push_back(bb);
            changed = 1;
        }
    }
    for (auto &bb : to_erase) {
        bb->erase_from_parent();
        delete bb;
    }
    return changed;
}

void DeadCode::mark(Function *func) {
    // TODO
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            marked[&inst] = false;
        }
    }
    
    // 将关键指令加入工作列表
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (is_critical(&inst)) {
                work_list.push_back(&inst);
                marked[&inst] = true;
            }
        }
    }
    
    // 广度优先标记所有被使用的指令
    while (!work_list.empty()) {
        auto *inst = work_list.front();
        work_list.pop_front();
        
        // 标记操作数的定义指令
        for (auto *op : inst->get_operands()) {
            if (op->get_type()->is_label_type()) {
                continue; // 跳过标签类型
            }
            
            if (auto *op_inst = dynamic_cast<Instruction *>(op)) {
                if (!marked[op_inst]) {
                    marked[op_inst] = true;
                    work_list.push_back(op_inst);
                }
            }
        }
    }
}

void DeadCode::mark(Instruction *ins) {
    // TODO
    if (!marked[ins]) {
        marked[ins] = true;
        work_list.push_back(ins);
    }
}

bool DeadCode::sweep(Function *func) {
    // TODO: 删除无用指令
    // 提示：
    // 1. 遍历函数的基本块，删除所有标记为true的指令
    // 2. 删除指令后，可能会导致其他指令的操作数变为无用，因此需要再次遍历函数的基本块
    // 3. 如果删除了指令，返回true，否则返回false
    // 4. 注意：删除指令时，需要先删除操作数的引用，然后再删除指令本身
    // 5. 删除指令时，需要注意指令的顺序，不能删除正在遍历的指令
    std::unordered_set<Instruction *> wait_del{};

    // 1. 收集所有未被标记的指令
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (!marked[&inst]) {
                wait_del.insert(&inst);
            }
        }
    }

    // 2. 执行删除
    for (auto *inst : wait_del) {
        inst->remove_all_operands();
        inst->get_parent()->erase_instr(inst);
        ins_count++;
    }
    
    return not wait_del.empty(); // changed
}

bool DeadCode::is_critical(Instruction *ins) {
    // TODO: 判断指令是否是无用指令
    // 提示：
    // 1. 如果是函数调用，且函数是纯函数，则无用
    // 2. 如果是无用的分支指令，则无用
    // 3. 如果是无用的返回指令，则无用
    // 4. 如果是无用的存储指令，则无用
    // 关键指令（不能被删除的指令）
    if (ins->is_ret() || ins->is_br() || ins->is_store() || ins->is_call()) {
        // 对于函数调用，如果函数不是纯函数，则是关键指令
        if (ins->is_call()) {
            auto *called_func = dynamic_cast<Function *>(ins->get_operand(0));
            if (called_func && func_info->is_pure_function(called_func)) {
                return false; // 纯函数调用不是关键指令
            }
        }
        return true; // 其他关键指令
    }
    
    return false; // 非关键指令
}

void DeadCode::sweep_globally() {
    std::vector<Function *> unused_funcs;
    std::vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m_->get_functions()) {
        if (f_r.get_use_list().size() == 0 and f_r.get_name() != "main")
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m_->get_global_variable()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    // changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m_->get_functions().erase(func);
    for (auto glob : unused_globals)
        m_->get_global_variable().erase(glob);
}
